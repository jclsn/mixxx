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

#include <stdio.h>
#include <stdlib.h>

#include "lut.h"

/* The number of bits to form the hash, which governs the overall size
 * of the hash lookup table, and hence the amount of chaining */

#define HASH_BITS 16

#define HASH(timecode) ((timecode) & ((1 << HASH_BITS) - 1))
#define NO_SLOT ((slot_no_t)-1)

/* Hash function that takes all 110-bits of the MK2s into account */

unsigned short HASH110(mk2bits_t *value) {

    /* Simple hash mixing using bit shifts and XORs */
    unsigned short hash = (unsigned short)(value->low ^ (value->low >> 16) ^ (value->low >> 32) ^ (value->low >> 48));
    hash ^= (unsigned short)(value->high ^ (value->high << 5) ^ (value->high >> 3));

    /* Final scrambling to improve distribution */
    hash ^= (hash >> 7) ^ (hash << 9);

    return hash;
}


/* Initialise an empty hash lookup table to store the given number
 * of timecode -> position lookups */

int lut_init(struct lut *lut, int nslots)
{
    int n, hashes;
    size_t bytes;

    hashes = 1 << HASH_BITS;
    bytes = sizeof(struct slot) * nslots + sizeof(slot_no_t) * hashes;

    fprintf(stderr, "Lookup table has %d hashes to %d slots"
            " (%d slots per hash, %zuKb)\n",
            hashes, nslots, nslots / hashes, bytes / 1024);

    lut->slot = (struct slot*)(malloc(sizeof(struct slot) * nslots));
    if (lut->slot == NULL) {
        perror("malloc");
        return -1;
    }

    lut->table = (slot_no_t*)(malloc(sizeof(slot_no_t) * hashes));
    if (lut->table == NULL) {
        perror("malloc");
        return -1;
    }

    for (n = 0; n < hashes; n++)
        lut->table[n] = NO_SLOT;

    lut->avail = 0;

    return 0;
}


/* MK2 version holding 110-bit integers as timecode */

int lut_init_mk2(struct lut_mk2 *lut, int nslots)
{
    int n, hashes;
    size_t bytes;

    hashes = 1 << HASH_BITS;
    bytes = sizeof(struct slot_mk2) * nslots + sizeof(slot_no_t) * hashes;

    fprintf(stderr, "Lookup table has %d hashes to %d slots"
            " (%d slots per hash, %zuKb)\n",
            hashes, nslots, nslots / hashes, bytes / 1024);

    lut->slot = malloc(sizeof(struct slot_mk2) * nslots);
    if (lut->slot == NULL) {
        perror("malloc");
        return -1;
    }

    lut->table = malloc(sizeof(slot_no_t) * hashes);
    if (lut->table == NULL) {
        perror("malloc");
        return -1;
    }

    for (n = 0; n < hashes; n++)
        lut->table[n] = NO_SLOT;

    lut->avail = 0;

    return 0;
}


void lut_clear(struct lut *lut)
{
    free(lut->table);
    free(lut->slot);
}


void lut_clear_mk2(struct lut_mk2 *lut)
{
    free(lut->table);
    free(lut->slot);
}


void lut_push(struct lut *lut, bits_t timecode)
{
    unsigned int hash;
    slot_no_t slot_no;
    struct slot *slot;

    slot_no = lut->avail++; /* take the next available slot */

    slot = &lut->slot[slot_no];
    slot->timecode = timecode;

    hash = HASH(timecode);
    slot->next = lut->table[hash];
    lut->table[hash] = slot_no;
}


/* MK2 version holding 110-bit integers as timecode */

void lut_push_mk2(struct lut_mk2 *lut, mk2bits_t *timecode)
{
    unsigned int hash;
    slot_no_t slot_no;
    struct slot_mk2 *slot;

    slot_no = lut->avail++; /* take the next available slot */

    slot = &lut->slot[slot_no];
    slot->timecode = *timecode;

    hash = HASH110(timecode);
    slot->next = lut->table[hash];
    lut->table[hash] = slot_no;
}


slot_no_t lut_lookup(struct lut *lut, bits_t timecode)
{
    unsigned int hash;
    slot_no_t slot_no;
    struct slot *slot;

    hash = HASH(timecode);
    slot_no = lut->table[hash];

    while (slot_no != NO_SLOT) {
        slot = &lut->slot[slot_no];
        if (slot->timecode == timecode)
            return slot_no;
        slot_no = slot->next;
    }

    return (slot_no_t)-1;
}


/* MK2 version holding 110-bit integers as timecode */

slot_no_t lut_lookup_mk2(struct lut_mk2 *lut, mk2bits_t *timecode)
{
    unsigned int hash;
    slot_no_t slot_no;
    struct slot_mk2 *slot;

    hash = HASH110(timecode);
    slot_no = lut->table[hash];

    while (slot_no != NO_SLOT) {
        slot = &lut->slot[slot_no];
        if (u128_eq(slot->timecode, *timecode))
            return slot_no;
        slot_no = slot->next;
    }

    return (slot_no_t)-1;
}
