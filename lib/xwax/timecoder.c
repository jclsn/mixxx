/*
 * Copyright (C) 2021 Mark Hills <mark@xwax.org>
 *
 * This file is part of "xwax".
 *
 * "xwax" is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * "xwax" is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#define _USE_MATH_DEFINES
#include <math.h>

#include "debug.h"
#include "filters.h"
#include "timecoder.h"

#define ZERO_THRESHOLD (128 << 16)

#define ZERO_RC 0.001 /* time constant for zero/rumble filter */

#define REF_PEAKS_AVG 48 /* in wave cycles */

/* The number of correct bits which come in before the timecode is
 * declared valid. Set this too low, and risk the record skipping
 * around (often to blank areas of track) during scratching */

#define VALID_BITS 24

#define MONITOR_DECAY_EVERY 512 /* in samples */

#define SQ(x) ((x)*(x))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*x))

/* Timecode definitions */

#define SWITCH_PHASE 0x1 /* tone phase difference of 270 (not 90) degrees */
#define SWITCH_PRIMARY 0x2 /* use left channel (not right) as primary */
#define SWITCH_POLARITY 0x4 /* read bit values in negative (not positive) */
#define TRAKTOR_MK2 0x8 /* use for Traktor MK2 timecode*/

static struct timecode_def timecodes[] = {
    {
        .name = "serato_2a",
        .desc = "Serato 2nd Ed., side A",
        .resolution = 1000,
        .bits = 20,
        .seed = 0x59017,
        .taps = 0x361e4,
        .length = 712000,
        .safe = 625000,
    },
    {
        .name = "serato_2b",
        .desc = "Serato 2nd Ed., side B",
        .resolution = 1000,
        .bits = 20,
        .seed = 0x8f3c6,
        .taps = 0x4f0d8, /* reverse of side A */
        .length = 922000,
        .safe = 908000,
    },
    {
        .name = "serato_cd",
        .desc = "Serato CD",
        .resolution = 1000,
        .bits = 20,
        .seed = 0xd8b40,
        .taps = 0x34d54,
        .length = 950000,
        .safe = 890000,
    },
    {
        .name = "traktor_a",
        .desc = "Traktor Scratch, side A",
        .resolution = 2000,
        .flags = SWITCH_PRIMARY | SWITCH_POLARITY | SWITCH_PHASE,
        .bits = 23,
        .seed = 0x134503,
        .taps = 0x041040,
        .length = 1500000,
        .safe = 605000,
    },
    {
        .name = "traktor_b",
        .desc = "Traktor Scratch, side B",
        .resolution = 2000,
        .flags = SWITCH_PRIMARY | SWITCH_POLARITY | SWITCH_PHASE,
        .bits = 23,
        .seed = 0x32066c,
        .taps = 0x041040, /* same as side A */
        .length = 2110000,
        .safe = 907000,
    },
    {
        .name = "traktor_mk2_a",
        .desc = "Traktor Scratch MK2, side A",
        .resolution = 2500,
        .flags = TRAKTOR_MK2,
        .bits = 110,
        .seed_mk2 = {
            .high = 0xc6007c63e,
            .low = 0x3fc00c60f8c1f00
        },
        .taps_mk2 = {
            .high = 0x400000000040,
            .low = 0x0000010800000001
        },
        .length = 1820000,
        .safe = 1800000,
    },    
    {
        .name = "traktor_mk2_b",
        .desc = "Traktor Scratch MK2, side B",
        .resolution = 2500,
        .flags = TRAKTOR_MK2,
        .bits = 110,
        .seed_mk2 = {
            .high = 0x1ff9f00003,
            .low = 0xe73ff00f9fe0c7c1
        },
        .taps_mk2 = {
            .high = 0x400000000040,
            .low = 0x0000010800000001
        },
        .length = 2570000,
        .safe = 2550000,
    },    
    {
        .name = "traktor_mk2_cd",
        .desc = "Traktor Scratch MK2, CD",
        .resolution = 3000,
        .flags = TRAKTOR_MK2,
        .bits = 110,
        .seed_mk2 = {
            .high = 0x7ce73,
            .low = 0xe0e0fff1fc1cf8c1
        },
        .taps_mk2 = {
            .high = 0x400000000000,
            .low = 0x1000010800000001
        },
        .length = 4500000,
        .safe = 4495000,
    },
    {
        .name = "mixvibes_v2",
        .desc = "MixVibes V2",
        .resolution = 1300,
        .flags = SWITCH_PHASE,
        .bits = 20,
        .seed = 0x22c90,
        .taps = 0x00008,
        .length = 950000,
        .safe = 655000,
    },
    {
        .name = "mixvibes_7inch",
        .desc = "MixVibes 7\"",
        .resolution = 1300,
        .flags = SWITCH_PHASE,
        .bits = 20,
        .seed = 0x22c90,
        .taps = 0x00008,
        .length = 312000,
        .safe = 238000,
    },
    {
        .name = "pioneer_a",
        .desc = "Pioneer RekordBox DVS Control Vinyl, side A",
        .resolution = 1000,
        .flags = SWITCH_POLARITY,
        .bits = 20,
        .seed = 0x78370,
        .taps = 0x7933a,
        .length = 635000,
        .safe = 614000,
    },
    {
        .name = "pioneer_b",
        .desc = "Pioneer RekordBox DVS Control Vinyl, side B",
        .resolution = 1000,
        .flags = SWITCH_POLARITY,
        .bits = 20,
        .seed = 0xf7012,
        .taps = 0x2ef1c,
        .length = 918500,
        .safe = 913000,
    },
};

/*
 * Calculate LFSR bit
 */

static inline bits_t lfsr(bits_t code, bits_t taps)
{
    bits_t taken;
    int xrs;

    taken = code & taps;
    xrs = 0;
    while (taken != 0x0) {
        xrs += taken & 0x1;
        taken >>= 1;
    }

    return xrs & 0x1;
}


static inline mk2bits_t lfsr_mk2(mk2bits_t code, mk2bits_t taps)
{
    mk2bits_t taken;
    mk2bits_t xrs;

    taken = u128_and(code, taps);
    xrs = U128_ZERO;

    while (u128_neq(taken, U128_ZERO)) {
        xrs = u128_add(xrs, u128_and(taken, U128_ONE));
        taken = u128_rshift(taken, 1);
    }

    return u128_and(xrs, U128_ONE);
}


/*
 * Linear Feedback Shift Register in the forward direction. New values
 * are generated at the least-significant bit.
 */

static inline bits_t fwd(bits_t current, struct timecode_def *def)
{
    bits_t l;

    /* New bits are added at the MSB; shift right by one */

    l = lfsr(current, def->taps | 0x1);
    return (current >> 1) | (l << (def->bits - 1));
}


static inline mk2bits_t fwd_mk2(mk2bits_t current, struct timecode_def *def)
{
    mk2bits_t l;

    /* New bits are added at the MSB; shift right by one */

    l = lfsr_mk2(current, u128_or(def->taps_mk2,  U128_ONE));
    return u128_or(u128_rshift(current, 1), u128_lshift(l, (def->bits - 1)));
}


/*
 * Linear Feedback Shift Register in the reverse direction
 */

static inline bits_t rev(bits_t current, struct timecode_def *def)
{
    bits_t l, mask;

    /* New bits are added at the LSB; shift left one and mask */

    mask = (1 << def->bits) - 1;
    l = lfsr(current, (def->taps >> 1) | (0x1 << (def->bits - 1)));
    return ((current << 1) & mask) | l;
}


static inline mk2bits_t rev_mk2(mk2bits_t current, struct timecode_def *def)
{
    mk2bits_t l, mask;

    /* New bits are added at the LSB; shift left one and mask */

    mask = u128_sub(u128_lshift(U128_ONE,  def->bits), U128_ONE);
    l = lfsr_mk2(current,
         u128_or(u128_rshift(def->taps_mk2, 1), 
         u128_lshift(U128_ONE, (def->bits - 1))));

    return u128_or(u128_and(u128_lshift(current, 1), mask), l);
}


/*
 * Where necessary, build the lookup table required for this timecode
 *
 * Return: -1 if not enough memory could be allocated, otherwise 0
 */

static int build_lookup(struct timecode_def *def)
{
    unsigned int n;
    bits_t current;

    if (def->lookup)
        return 0;

    fprintf(stderr, "Building LUT for %d bit %dHz timecode (%s)\n",
            def->bits, def->resolution, def->desc);

    if (lut_init(&def->lut, def->length) == -1)
	return -1;

    current = def->seed;

    for (n = 0; n < def->length; n++) {
        bits_t next;

        assert(lut_lookup(&def->lut, current) == (unsigned)-1);
        /* timecode must not wrap */
        lut_push(&def->lut, current);

        /* check symmetry of the lfsr functions */
        next = fwd(current, def);
        assert(rev(next, def) == current);

        current = next;
    }

    def->lookup = true;

    return 0;
}


static int build_lookup_mk2(struct timecode_def *def)
{
    unsigned int n;
    mk2bits_t current, next;

    if (def->lookup)
        return 0;

    fprintf(stderr, "Building LUT for %d bit %dHz timecode (%s)\n",
            def->bits, def->resolution, def->desc);

    if (lut_init_mk2(&def->lut_mk2, def->length) == -1)
        return -1;

    current = def->seed_mk2;

    for (n = 0; n < def->length; n++) {

        /* timecode must not wrap */
        assert(lut_lookup_mk2(&def->lut_mk2, &current) == (unsigned)-1);
        lut_push_mk2(&def->lut_mk2, &current);

        /* check symmetry of the lfsr functions */
        next = fwd_mk2(current, def);
        assert(u128_eq(rev_mk2(next, def), current));

        current = next;
    }

    def->lookup = true;

    return 0;
}

int set_path(char *path, char *subpath)
{
    if (!path || !subpath)
        return -1;

    const char *confdir = "/.mixxx";
    const char *home;
    int size;
    int len;

    home = getenv("HOME");
    if (!home)
        return -1;

    len = strlen(home) + strlen(confdir) + strlen(subpath);
    size = snprintf(path, len + 1, "%s%s%s", home, confdir, subpath);
    if (size != len) {
        perror("snprintf");
        return -1;
    }

    return 0;
}

int lut_store_mk2(struct timecode_def *def)
{
    if (!def)
        return -1;

    struct slot_mk2 *slot;
    struct stat stats;
    slot_no_t *hash;

    int i, j, len, hashes;
    char filename[1024];
    char path[1024];
    FILE *fp = NULL;
    int r = 0;
    int size;

    if (set_path(path, "/lut"))
        return -1;

    if (stat(path, &stats)) {
        if (mkdir(path, 0755)) {
            perror("mkdir");
            return -1;
        }
    }

    sprintf(filename, "%s%s%s", "/lut/", def->name, ".lut");
    if (set_path(path, filename))
        return -1;

    printf("Storing LUT at %s\n", path);
    fp = fopen(path, "w");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    for (i = 0; i < def->length; i++) {
        slot = &def->lut_mk2.slot[i];

        if (!slot) {
            printf("slot_no: %d doesn't exist'\n", i);
            r = -1;
            goto out;
        }
        size = fwrite(slot, sizeof(struct slot_mk2), 1, fp);

        if(!size) {
            perror("fwrite");
            r = -1;
            goto out;
        }
    }

    hashes = 1 << 16;
    for (j = 0; j < hashes; j++) {
        hash = &def->lut_mk2.table[j];

        size = fwrite(hash, sizeof(slot_no_t), 1, fp);
        if(!size) {
            perror("fwrite");
            r = -1;
            goto out;
        }
    }

    size = fwrite(&def->lut_mk2.avail, sizeof(slot_no_t), 1, fp);
    if(!size) {
        perror("fwrite");
        r = -1;
        goto out;
    }

out:
    fclose(fp);

    if (hashes == j && def->length == i)
        printf("Success: ");
    else
        printf("Something went wrong: ");

    printf("Wrote %d hashes and %d slots to disk\n", j, i);

    return r;
}

int lut_load_mk2(struct timecode_def *def)
{
    if (!def)
        return -1;

    struct slot_mk2 *slot;

    char filename[1024];
    int i, j, hashes;
    char path[1024];
    int r = 0;
    int size;
    FILE *fp;
    int len;

    sprintf(filename, "%s%s%s", "/lut/", def->name, ".lut");
    if (set_path(path, filename))
        return -1;

    fp = fopen(path, "r");
    if (!fp) {
        printf("LUT for %s not found on disk\n", def->desc);
        return -1;
    }

    r = lut_init_mk2(&def->lut_mk2, def->length);
    if (r) {
        printf("Couldn't initialise LUT\n");
        goto out;
    }

    printf("Loading LUT from %s\n", path);
    for (i = 0; i < def->length; i++) {
        slot = &def->lut_mk2.slot[i];

        size = fread(slot, sizeof(struct slot_mk2), 1, fp);
        if(!size) {
            perror("fread");
            r = -1;
            goto out;
        }
    }

    hashes = 1 << 16;
    for (j = 0; j < hashes; j++) {

        slot_no_t *hash = &def->lut_mk2.table[j];

        size = fread(hash, sizeof(slot_no_t), 1, fp);
        if(!size) {
            perror("fread");
            r = -1;
            goto out;
        }
    }

    size = fread(&def->lut_mk2.avail, sizeof(slot_no_t), 1, fp);
    if(!size) {
        perror("fwrite");
        r = -1;
    }
        goto out;


out:
    fclose(fp);

    if (hashes == j && def->length == i) {
        def->lookup = true;
        printf("Success: ");
    } else {
        printf("Something went wrong: ");
    }

    printf("Loaded %d slots and %d hashes into memory\n", i, j);

    return r;
}


/*
 * Find a timecode definition by name
 *
 * Return: pointer to timecode definition, or NULL if not available
 */

struct timecode_def* timecoder_find_definition(const char *name)
{
    unsigned int n;

    for (n = 0; n < ARRAY_SIZE(timecodes); n++) {
        struct timecode_def *def = &timecodes[n];

        if (strcmp(def->name, name) != 0)
            continue;

        if (!def->lookup) {
            if (def->flags & TRAKTOR_MK2) {
                if (!lut_load_mk2(def))
                    return def;

                if (build_lookup_mk2(def) == -1)
                    return NULL;  /* error */

                if(lut_store_mk2(def)) {
                    timecoder_free_lookup();
                    printf("Couldn't store LUT on disk\n");
                    return NULL;
                }

            } else {
                if (build_lookup(def) == -1)
                    return NULL;  /* error */
            }
        }

        return def;
    }

    return NULL;  /* not found */
}

/*
 * Free the timecoder lookup tables when they are no longer needed
 */

void timecoder_free_lookup(void) {
    unsigned int n;

    for (n = 0; n < ARRAY_SIZE(timecodes); n++) {
        struct timecode_def *def = &timecodes[n];

        if (def->flags & TRAKTOR_MK2) {
            if (def->lookup)
                lut_clear_mk2(&def->lut_mk2);
        } else {
            if (def->lookup)
                lut_clear(&def->lut);
        }
    }
}

/*
 * Initialise a subcode decoder for the Traktor MK2
 */

void mk2_subcode_init(struct mk2_subcode *sc)
{
    sc->valid_counter = 0;
    sc->avg_reading = INT_MAX/2;
    sc->avg_slope = INT_MAX/2;
    sc->bit = U128_ZERO;
    delayline_init(&sc->readings);
}

/*
 * Initialise filter values for the MK2 demodulation
 */

static void init_mk2_channel(struct timecoder_channel *ch)
{
    ch->mk2.deriv_scaled = INT_MAX/2;
    ch->mk2.rms_old = INT_MAX/2;
    ch->mk2.rms = INT_MAX/2;
    ch->mk2.rms_deriv = 0;

    delayline_init(&ch->mk2.delayline);
}


/*
 * Initialise filter values for one channel
 */

static void init_channel(struct timecode_def *def, struct timecoder_channel *ch)
{
    ch->positive = false;
    ch->zero = 0;

    if (def->flags & TRAKTOR_MK2)
        init_mk2_channel(ch);
}

/*
 * Initialise a timecode decoder at the given reference speed
 *
 * Return: -1 if the timecoder could not be initialised, otherwise 0
 */

void timecoder_init(struct timecoder *tc, struct timecode_def *def,
                    double speed, unsigned int sample_rate, bool phono)
{
    assert(def != NULL);

    /* A definition contains a lookup table which can be shared
     * across multiple timecoders */

    assert(def->lookup);
    tc->def = def;
    tc->speed = speed;

    tc->dt = 1.0 / sample_rate;
    tc->sample_rate = sample_rate;
    tc->zero_alpha = tc->dt / (ZERO_RC + tc->dt);
    tc->threshold = ZERO_THRESHOLD;
    if (phono)
        tc->threshold >>= 5; /* approx -36dB */

    tc->forwards = 1;
    init_channel(tc->def, &tc->primary);
    init_channel(tc->def, &tc->secondary);
    pitch_init(&tc->pitch, tc->dt);

    tc->ref_level = INT_MAX;
    tc->bitstream = 0;
    tc->timecode = 0;
    tc->valid_counter = 0;
    tc->timecode_ticker = 0;

    tc->mon = NULL;

    /* Compute the factor the scale up the derivative to the original level */
    tc->gain_compensation = 1.0 / (M_PI * tc->def->resolution / tc->sample_rate);

    mk2_subcode_init(&tc->upper_subcode);
    mk2_subcode_init(&tc->lower_subcode);
}

/*
 * Clear resources associated with a timecode decoder
 */

void timecoder_clear(struct timecoder *tc)
{
    assert(tc->mon == NULL);

	if (tc->def->flags & TRAKTOR_MK2) {
		delayline_init(&tc->primary.mk2.delayline);
		delayline_init(&tc->secondary.mk2.delayline);
		delayline_init(&tc->upper_subcode.readings);
		delayline_init(&tc->lower_subcode.readings);
	}

}

/*
 * Initialise a raster display of the incoming audio
 *
 * The monitor (otherwise known as 'scope' in the interface) is an x-y
 * display of the post-calibrated incoming audio.
 *
 * Return: -1 if not enough memory could be allocated, otherwise 0
 */

int timecoder_monitor_init(struct timecoder *tc, int size)
{
    assert(tc->mon == NULL);
    tc->mon_size = size;
    tc->mon = (unsigned char*)(malloc(SQ(tc->mon_size)));
    if (tc->mon == NULL) {
        perror("malloc");
        return -1;
    }
    memset(tc->mon, 0, SQ(tc->mon_size));
    tc->mon_counter = 0;
    return 0;
}

/*
 * Clear the monitor on the given timecoder
 */

void timecoder_monitor_clear(struct timecoder *tc)
{
    assert(tc->mon != NULL);
    free(tc->mon);
    tc->mon = NULL;
}

/*
 * Update channel information with axis-crossings
 */

static void detect_zero_crossing(struct timecoder_channel *ch,
                                 signed int v, double alpha,
                                 signed int threshold)
{
    ch->crossing_ticker++;

    ch->swapped = false;
    if (v > ch->zero + threshold && !ch->positive) {
        ch->swapped = true;
        ch->positive = true;
        ch->crossing_ticker = 0;
    } else if (v < ch->zero - threshold && ch->positive) {
        ch->swapped = true;
        ch->positive = false;
        ch->crossing_ticker = 0;
    }

    ch->zero += alpha * (v - ch->zero);
}

/*
 * Plot the given sample value in the x-y monitor
 */

static inline void update_monitor(struct timecoder *tc, signed int x, signed int y)
{
    int px, py, size, ref;

    if (!tc->mon)
        return;

    size = tc->mon_size;
    ref = tc->ref_level;

    /* Decay the pixels already in the montior */

    if (++tc->mon_counter % MONITOR_DECAY_EVERY == 0) {
        int p;

        for (p = 0; p < SQ(size); p++) {
            if (tc->mon[p])
                tc->mon[p] = tc->mon[p] * 7 / 8;
        }
    }

    assert(ref > 0);

    /* ref_level is half the precision of signal level */
    px = size / 2 + (long long)x * size / ref / 8;
    py = size / 2 + (long long)y * size / ref / 8;

    if (px < 0 || px >= size || py < 0 || py >= size)
        return;

    tc->mon[py * size + px] = 0xff; /* white */
}


#define FORWARD_FACTOR 1.5
#define REVERSE_FACTOR 1.75
static inline void detect_bit_flip(const int slope[2], int rms, int reading, int avg_reading,
                                   mk2bits_t *bit, bool *bit_flipped, bool forwards, mk2bits_t one)
{
    double threshold;

    if (*bit_flipped == false) {
        if (forwards) {
            threshold = rms / FORWARD_FACTOR;
        } else {
            threshold = rms / REVERSE_FACTOR;
            one = u128_not(one);
        }

        if (u128_eq(*bit, u128_not(one)) && slope[0] > threshold && slope[1] > threshold) {
            *bit = one;
            *bit_flipped = true;
        } else if (u128_eq(*bit, one) && slope[0] < -threshold && slope[1] < -threshold) {
            *bit = u128_not(one);
            *bit_flipped = true;
        }
    } else {
        *bit_flipped = false;
    }
}


static inline bool lfsr_verify(struct timecode_def *def, mk2bits_t *timecode, mk2bits_t *bitstream, 
        mk2bits_t bit, bool forwards)
{
    if (forwards) {
        *timecode = fwd_mk2(*timecode, def);
        *bitstream = u128_add(u128_rshift(*bitstream, 1), u128_lshift(bit, (def->bits - 1)));
    } else {
        mk2bits_t mask = u128_sub(u128_lshift(U128_ONE, def->bits), U128_ONE);
        *timecode = rev_mk2(*timecode, def);
        *bitstream = u128_add(u128_and(u128_lshift(*bitstream, 1), mask), bit);
    }

    if (u128_eq(*timecode, *bitstream))
        return true;
    else
        return false;
}


inline static void mk2_process_subcode(struct timecoder *tc, struct mk2_subcode *sc, signed int reading)
{
        int current_slope[2];

        delayline_push(&sc->readings, reading);
        sc->avg_reading = ema(reading, &sc->avg_reading, 0.01);

        /* Calculate absolute of average slope */
        sc->avg_slope =
                ema(abs(reading - *delayline_at(&sc->readings, 1)),
                    &sc->avg_slope,
                    0.01);

        /* Calculate current and last slope */
        current_slope[0] =  (reading - *delayline_at(&sc->readings, 1));
        current_slope[1] =  (reading - *delayline_at(&sc->readings, 2));

        /* The bits only change when an offset jump occurs. Else the previous bit is taken  */
        detect_bit_flip(current_slope, tc->secondary.mk2.rms, reading, sc->avg_reading, &sc->bit,
                        &sc->recent_bit_flip, tc->forwards, U128(0x0, !tc->secondary.positive));
        
        if (lfsr_verify(tc->def, &sc->timecode, &sc->bitstream, sc->bit, tc->forwards)) {
            (sc->valid_counter)++;
        } else {
            sc->timecode = sc->bitstream;
            sc->valid_counter = 0;
        }
}


static void mk2_process_bitstream(struct timecoder *tc, signed int reading) {

    /* 
     * Detect if the offset jumps on upper and lower bitstream
     */
    if (tc->secondary.positive)
        mk2_process_subcode(tc, &tc->upper_subcode, reading);
    else if (!tc->secondary.positive)
        mk2_process_subcode(tc, &tc->lower_subcode, reading);

    if (tc->lower_subcode.valid_counter > tc->upper_subcode.valid_counter) {
        tc->mk2_bitstream = tc->lower_subcode.bitstream;
        tc->mk2_timecode = tc->lower_subcode.timecode;
    } else {
        tc->mk2_bitstream = tc->upper_subcode.bitstream;
        tc->mk2_timecode = tc->upper_subcode.timecode;
    }

    if (u128_eq(tc->mk2_timecode, tc->mk2_bitstream)) {
        tc->valid_counter++;
    } else {
        tc->timecode = tc->bitstream;
        tc->valid_counter = 0;
    }
    /* Take note of the last time we read a valid timecode */

    tc->timecode_ticker = 0;

    tc->ref_level -= tc->ref_level / REF_PEAKS_AVG;
    tc->ref_level += abs((int) (tc->secondary.mk2.rms_deriv * tc->gain_compensation)) / REF_PEAKS_AVG;

    debug("upper.valid_counter: %d, lower.valid_counter %d, forwards: %b\n", */
           tc->upper.valid_counter,
           tc->lower.valid_counter,
           tc->forwards);
}


/*
 * Extract the bitstream from the sample value
 */

static void process_bitstream(struct timecoder *tc, signed int m)
{
    bits_t b;

    b = m > tc->ref_level;

    /* Add it to the bitstream, and work out what we were expecting
     * (timecode). */

    /* tc->bitstream is always in the order it is physically placed on
     * the vinyl, regardless of the direction. */

    if (tc->forwards) {
	tc->timecode = fwd(tc->timecode, tc->def);
	tc->bitstream = (tc->bitstream >> 1)
	    + (b << (tc->def->bits - 1));

    } else {
	bits_t mask;

	mask = ((1 << tc->def->bits) - 1);
	tc->timecode = rev(tc->timecode, tc->def);
	tc->bitstream = ((tc->bitstream << 1) & mask) + b;
    }

    if (tc->timecode == tc->bitstream)
	tc->valid_counter++;
    else {
	tc->timecode = tc->bitstream;
	tc->valid_counter = 0;
    }

    /* Take note of the last time we read a valid timecode */

    tc->timecode_ticker = 0;

    /* Adjust the reference level based on this new peak */

    tc->ref_level -= tc->ref_level / REF_PEAKS_AVG;
    tc->ref_level += m / REF_PEAKS_AVG;

    debug("%+6d zero, %+6d (ref %+6d)\t= %d%c (%5d)",
          tc->primary.zero,
          m, tc->ref_level,
	  b, tc->valid_counter == 0 ? 'x' : ' ',
	  tc->valid_counter);
}

/*
 * Process a single sample from the incoming audio
 *
 * The two input signals (primary and secondary) are in the full range
 * of a signed int; ie. 32-bit signed.
 */

#define ALPHA_EMA 3e-1
static void process_sample(struct timecoder *tc,
			   signed int primary, signed int secondary)
{
    if (tc->def->flags & TRAKTOR_MK2) {

        /* Compute the discrete derivative */
        tc->primary.mk2.deriv[0] = derivative(ema(primary, &tc->primary.mk2.ema, ALPHA_EMA), &tc->primary.mk2.deriv[1]);
        tc->secondary.mk2.deriv[0] = derivative(ema(secondary, &tc->secondary.mk2.ema, ALPHA_EMA), &tc->secondary.mk2.deriv[1]);

        /* Compute the smoothed RMS value */
        tc->primary.mk2.rms = rms(primary, &tc->primary.mk2.rms_old);
        tc->secondary.mk2.rms = rms(secondary, &tc->secondary.mk2.rms_old);

        /* Compute the smoothed RMS value for the derivative */
        tc->primary.mk2.rms_deriv = rms(tc->primary.mk2.deriv[0], &tc->primary.mk2.rms_deriv_old);
        tc->secondary.mk2.rms_deriv = rms(tc->secondary.mk2.deriv[0], &tc->secondary.mk2.rms_deriv_old);

        /* Compute the gain compensation for the derivative*/
        tc->gain_compensation = (double) tc->secondary.mk2.rms / tc->secondary.mk2.rms_deriv;
        if (tc->gain_compensation > 30.0) // without this limit pitch becomes too sensitive
            tc->gain_compensation = 30.0;

        tc->dB = 20 * log10((double) tc->secondary.mk2.rms / INT_MAX);

        /* Compute the scaled derivative */
        tc->primary.mk2.deriv_scaled = tc->primary.mk2.deriv[0] * tc->gain_compensation;
        tc->secondary.mk2.deriv_scaled = tc->secondary.mk2.deriv[0] * tc->gain_compensation;

        detect_zero_crossing(&tc->primary, tc->primary.mk2.deriv_scaled, tc->zero_alpha, tc->threshold);
        detect_zero_crossing(&tc->secondary, tc->secondary.mk2.deriv_scaled, tc->zero_alpha, tc->threshold);

    } else {
        detect_zero_crossing(&tc->primary, primary, tc->zero_alpha, tc->threshold);
        detect_zero_crossing(&tc->secondary, secondary, tc->zero_alpha, tc->threshold);
    }

    /* If an axis has been crossed, use the direction of the crossing
     * to work out the direction of the vinyl */

    if (tc->primary.swapped || tc->secondary.swapped) {
        bool forwards;

        if (tc->primary.swapped) {
            forwards = (tc->primary.positive != tc->secondary.positive);
        } else {
            forwards = (tc->primary.positive == tc->secondary.positive);
        }

        if (tc->def->flags & SWITCH_PHASE)
	    forwards = !forwards;

        if (forwards != tc->forwards) { /* direction has changed */
            tc->forwards = forwards;
            tc->valid_counter = 0;
        }
    }

    /* If any axis has been crossed, register movement using the pitch
     * counters */

    if (!tc->primary.swapped && !tc->secondary.swapped)
	pitch_dt_observation(&tc->pitch, 0.0);
    else {
	double dx;

	dx = 1.0 / tc->def->resolution / 4;
	if (!tc->forwards)
	    dx = -dx;
	pitch_dt_observation(&tc->pitch, dx);
    }

    /* If we have crossed the primary channel in the right polarity,
     * it's time to read off a timecode 0 or 1 value */

    if (tc->def->flags & TRAKTOR_MK2) {
        if (tc->secondary.swapped)
        {
            int reading = *delayline_at(&tc->secondary.mk2.delayline, 3);
            mk2_process_bitstream(tc, reading);
        }
    } else {
        if (tc->secondary.swapped &&
           tc->primary.positive == ((tc->def->flags & SWITCH_POLARITY) == 0))
        {
            signed int m;

            /* scale to avoid clipping */
            m = abs(primary / 2 - tc->primary.zero / 2);
            process_bitstream(tc, m);
        }
    }

    tc->timecode_ticker++;
}

/*
 * Cycle to the next timecode definition which has a valid lookup
 *
 * Return: pointer to timecode definition
 */

static struct timecode_def* next_definition(struct timecode_def *def)
{
    assert(def != NULL);

    do {
        def++;

        if (def >= timecodes + ARRAY_SIZE(timecodes))
            def = timecodes;

    } while (!def->lookup);

    return def;
}

/*
 * Change the timecode definition to the next available
 */

void timecoder_cycle_definition(struct timecoder *tc)
{
    tc->def = next_definition(tc->def);
    tc->valid_counter = 0;
    tc->timecode_ticker = 0;
}

/*
 * Submit and decode a block of PCM audio data to the timecode decoder
 *
 * PCM data is in the full range of signed short; ie. 16-bit signed.
 */

void timecoder_submit(struct timecoder *tc, signed short *pcm, size_t npcm)
{
    while (npcm--) {
	signed int left, right, primary, secondary;

        left = pcm[0] << 16;
        right = pcm[1] << 16;

        if (tc->def->flags & SWITCH_PRIMARY) {
            primary = left;
            secondary = right;
        } else {
            primary = right;
            secondary = left;
        }

        if (tc->def->flags & TRAKTOR_MK2) {
            /* Push the samples into the ringbuffer */
            delayline_push(&tc->primary.mk2.delayline, primary);
            delayline_push(&tc->secondary.mk2.delayline, secondary);

            process_sample(tc, primary, secondary);

            /* Display the derivative in the monitor */
            update_monitor(tc, tc->primary.mk2.deriv_scaled << 1, tc->secondary.mk2.deriv_scaled << 1);
        } else {
            process_sample(tc, primary, secondary);
            update_monitor(tc, left, right);
        }

        pcm += TIMECODER_CHANNELS;
    }
}

/*
 * Get the last-known position of the timecode
 *
 * If now data is available or if too few bits have been error
 * checked, then this counts as invalid. The last known position is
 * given along with the time elapsed since the position stamp was
 * read.
 *
 * Return: the known position of the timecode, or -1 if not known
 * Post: if when != NULL, *when is the elapsed time in seconds
 */

signed int timecoder_get_position(struct timecoder *tc, double *when)
{
    signed int r;

    if (tc->valid_counter <= VALID_BITS)
        return -1;

    if (tc->def->flags & TRAKTOR_MK2) {
        r = lut_lookup_mk2(&tc->def->lut_mk2, &tc->mk2_bitstream);
        if (r == -1)
            return -1;
    } else {
        r = lut_lookup(&tc->def->lut, tc->bitstream);
        if (r == -1)
            return -1;
    }

    if (r >= 0) {
        // normalize position to milliseconds, not timecode steps -- Owen
        r = r * (1000.0 / ((double)tc->def->resolution * tc->speed));
    }

    if (when)
        *when = tc->timecode_ticker * tc->dt;

    return r;
}
