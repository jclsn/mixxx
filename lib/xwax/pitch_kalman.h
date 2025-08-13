/*
 * Kalman filter pitch estimator with sensitivity mode switches
 *
 * Model: Constant velocity (x, v) with acceleration process noise
 *
 * State:
 *   x : The current position in time
 *   v : Velocity (pitch)
 *
 * Assumption: :
 *   dx: We advanced a quarter of the cycle
 *
 * Observation:
 *   z = dx (new measurement after the interval dt)
 *
 * Modes:
 *   stable  : Low Q and high R for stable playback
 *   medium  : Medium values for slight pitch changes
 *   reactive: High Q and low R for high reactivity (scratching)
 */

#ifndef PITCH_KALMAN_H
#define PITCH_KALMAN_H

#include <errno.h>
#include <stdio.h>

enum { x = 0, v = 1 }; /* Matrix indices for readability */

/* Struct for the Kalman filter coefficients R and Q */

struct kalman_coeffs {
    double Q;
    double R;
};

#define KALMAN_COEFFS(q_arg, r_arg) \
    (struct kalman_coeffs) {.Q = (q_arg), .R = (r_arg)}

struct pitch_kalman {
    /*
     * NOTE: In discrete time dt is usually denoted as Ts = 1/Fs,
     * but xwax uses the continuous notation.
     */

    double dt; /* Sampling interval: (s) */

    double Xk[2]; /* Position and velocity state space */

    /* Covariance P (2x2, symmetric) */

    double P[2][2];

    /* Tresholds of the innovation quantity for the mode switches */

    double scratch_threshold, medium_threshold;

    /* Currently used coefficients*/

    struct kalman_coeffs* coeffs;

    /* Stable, medium reactive coefficients for the mode switch */

    struct kalman_coeffs stable;
    struct kalman_coeffs medium;
    struct kalman_coeffs reactive;
};

void pitch_kalman_init(struct pitch_kalman* p, double dt, struct kalman_coeffs stable,
        struct kalman_coeffs medium, struct kalman_coeffs reactive,
        double medium_threshold, double scratch_threshold);
void pitch_kalman_update(struct pitch_kalman* p, double dx);

/*
 * Retune noise sensitivity without resetting state
 */

static inline void kalman_tune_sensitivity(struct pitch_kalman* p, struct kalman_coeffs* coeffs) {
    if (!p || !coeffs) {
        errno = EINVAL;
        perror(__func__);
        return;
    }

    p->coeffs = coeffs;
}


/*
 * Get the current pitch (velocity estimate)
 */

static inline double pitch_kalman_current(const struct pitch_kalman* p) {

    if (!p) {
        errno = EINVAL;
        perror(__func__);
        return 0.0;
    }

    return p->Xk[v];
}

#endif /* PITCH_KALMAN_H */
