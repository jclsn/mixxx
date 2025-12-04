
#include <limits.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "filters.h"

/*
 * Initializes the exponential moving average filter.
 */

void ema_init(struct ema_filter *filter, const double alpha)
{
    if (!filter) {
        errno = EINVAL; perror(__func__);
        return;
    }

    filter->alpha = alpha;
    filter->y_old = 0;
}

/*
 * Computes an exponential moving average with the possibility to weight newly added
 * values with a factor alpha.
 */

int ema(struct ema_filter *filter, const int x)
{
    if (!filter) {
        errno = EINVAL;
        perror(__func__);
        return -EINVAL;
    }

    int y = filter->alpha * x + (1 - filter->alpha) * filter->y_old;
    filter->y_old = y;

    return y;
}


/*
 * Initializes the exponential moving average filter.
 */

void emaf_init(struct emaf_filter *filter, const double alpha)
{
    if (!filter) {
        errno = EINVAL;
        perror(__func__);
        return;
    }

    filter->alpha = alpha;
    filter->y_old = 0;
}

/*
 * Computes an exponential moving average with the possibility to weight newly added
 * values with a factor alpha.
 */

int emaf(struct emaf_filter *filter, const double x)
{
    if (!filter) {
        errno = EINVAL;
        perror(__func__);
        return -EINVAL;
    }

    double y = filter->alpha * x + (1 - filter->alpha) * filter->y_old;
    filter->y_old = y;

    return y;
}


/*
 * Initializes the derivative filter.
 */

void derivative_init(struct differentiator *filter)
{
    if (!filter) {
        errno = EINVAL;
        perror(__func__);
        return;
    }

    filter->x_old = 0;
}

/*
 * Computes a simple derivative, i.e. the slope of the input signal without gain compensation.
 */

int derivative(struct differentiator *filter, const int x)
{
    int y = x - filter->x_old;
    filter->x_old = x;

    return y;
}

/*
 * Initializes the RMS filter
 */

void rms_init(struct root_mean_square *filter, const float alpha)
{
    if (!filter) {
        errno = EINVAL;
        perror(__func__);
        return;
    }

    filter->squared_old = 0;
    filter->alpha = alpha;
}

/*
 * Computes the RMS value over a running sum.
 * The 1.0 > alpha > 0 determines the smoothness of the result:
 */

int rms(struct root_mean_square *filter, const int x)
{
    if (!filter) {
        errno = EINVAL;
        perror(__func__);
        return -EINVAL;
    }

    /* Compute squared value */
    unsigned long long squared = (unsigned long long)x * (unsigned long long)x;

    /* Apply EMA filter to squared values */
    filter->squared_old = (1.0 - filter->alpha) * filter->squared_old + filter->alpha * squared;

    /* Take square root at the end */
    return (int)sqrt(filter->squared_old);
}

/*
 * Initializes the bandpass fillter
 */


void apbp_init(struct apbp_filter *filter, double Fc, double Fb, unsigned int sample_rate)
{
    double wb = 2 * Fb / (double) sample_rate;
    double wc = 2 * Fc / (double) sample_rate;

    filter->c = (tan(M_PI * wb / 2) - 1) / (tan(M_PI * wb / 2) + 1);
    filter->d = -cos(M_PI * wc);
    filter->xh[0] = 0;
    filter->xh[1] = 0;
}

/*
 * Applies a bandpass filter to the input signal x
 *
 * ωc is the normalized center frequency 0<ωc<1, i.e. 2*fc/fS
 * ωb is the normalized bandwidth 0<ωb<1, i.e. 2*fb/fS
 */

int apbp(struct apbp_filter *filter, int x)
{
    int xh_new = x - filter->d * (1 - filter->c) * filter->xh[1] + filter->c * filter->xh[2];
    int ap_y = -filter->c * xh_new + filter->d * (1 - filter->c) * filter->xh[1] + filter->xh[2];

    filter->xh[1] = filter->xh[0];
    filter->xh[0] = xh_new;

    return 0.5 * (x - ap_y); // change to plus for bandreject
}

// Initialize the butterworth filter
void butterworth_init(struct butterworth_filter* f, const double b[5], const double a[5])
{
    for(int i=0;i<5;i++){
        f->b[i] = b[i];
        f->a[i] = a[i];
        f->x[i] = 0.0;
        f->y[i] = 0.0;
    }
}

// Process one sample
double butterworth(struct butterworth_filter* f, double xn)
{
    // shift old samples
    for(int i=4;i>0;i--){
        f->x[i] = f->x[i-1];
        f->y[i] = f->y[i-1];
    }

    f->x[0] = xn;

    double yn = 0.0;
    for(int i=0;i<5;i++){
        yn += f->b[i]*f->x[i];
    }

    for(int i=1;i<5;i++){
        yn -= f->a[i]*f->y[i];
    }

    f->y[0] = yn;

    return yn;
}

// Initialize filter
struct iir_filter* iir_init(int ord, const double* b, const double* a)
{
    struct iir_filter* f = (struct iir_filter*)malloc(sizeof(struct iir_filter));

    f->ord = ord;
    f->b = (double*)malloc((ord + 1) * sizeof(double));
    f->a = (double*)malloc((ord + 1) * sizeof(double));
    f->x = (double*)calloc(ord + 1, sizeof(double));
    f->y = (double*)calloc(ord + 1, sizeof(double));

    for (int i = 0; i <= ord; i++) {
        f->b[i] = b[i];
        f->a[i] = a[i];
    }
    return f;
}

// Free filter memory
void iir_free(struct iir_filter* f)
{
    free(f->b);
    free(f->a);
    free(f->x);
    free(f->y);
    free(f);
}

// Process one sample
double iir_filter(struct iir_filter* f, double xn)
{
    // shift old samples
    for (int i = f->ord; i > 0; i--) {
        f->x[i] = f->x[i - 1];
        f->y[i] = f->y[i - 1];
    }

    f->x[0] = xn;

    double yn = 0.0;
    for (int i = 0; i <= f->ord; i++) {
        yn += f->b[i] * f->x[i];
    }

    for (int i = 1; i <= f->ord; i++) {
        yn -= f->a[i] * f->y[i];
    }

    f->y[0] = yn;

    return yn;
}
