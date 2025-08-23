#ifndef FREQ_KALMAN_H
#define FREQ_KALMAN_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Constant-acceleration Kalman on frequency
 * State x = [ f, fdot ]^T
 * Units: f in Hz, fdot in Hz/s
 */
struct kalman_freq {
    double dt;   /* sample interval (s) */

    /* state */
    double f;       /* Hz */
    double fdot;    /* Hz/s */

    /* covariance (symmetric 2x2) */
    double P00, P01, P11;

    /* noise parameters */
    double q;   /* process noise spectral density (Hz^2 / s^3) */
    double r;   /* measurement variance (Hz^2) */

    bool initialized;
};

/* Initialize
 *   dt: seconds per sample
 *   f0: initial frequency (Hz)
 *   q : process noise spectral density
 *   r : measurement variance (variance of your IF in Hz^2)
 */
static inline void fk_init(struct kalman_freq *k, double dt, double f0, double q, double r)
{
    k->dt = dt;
    k->f = f0;
    k->fdot = 0.0;

    /* large initial uncertainty so early measurements dominate */
    k->P00 = 1e6;
    k->P01 = 0.0;
    k->P11 = 1e6;

    k->q = q;
    k->r = r;
    k->initialized = true;
}

/* One update with instantaneous-frequency measurement z (Hz).
 * Returns the filtered frequency (Hz).
 */
static inline double fk_update(struct kalman_freq *k, double z)
{
    /* ===== Predict =====
       F = [1 dt; 0 1] ; Q = q * [dt^3/3 dt^2/2; dt^2/2 dt] */
    const double dt = k->dt;

    /* state prediction */
    const double f_pred    = k->f    + dt * k->fdot;
    const double fdot_pred = k->fdot;

    /* covariance prediction */
    const double Q00 = k->q * (dt*dt*dt / 3.0);
    const double Q01 = k->q * (dt*dt / 2.0);
    const double Q11 = k->q * dt;

    const double P00 = k->P00 + dt*(k->P01 + k->P01) + dt*dt*k->P11 + Q00;
    const double P01 = k->P01 + dt*k->P11 + Q01;
    const double P11 = k->P11 + Q11;

    /* ===== Update =====
       H = [1 0], S = P00 + r, K = [P00/S, P01/S] */
    const double y   = z - f_pred;           /* innovation */
    const double S   = P00 + k->r;
    const double invS= 1.0 / S;
    const double K0  = P00 * invS;
    const double K1  = P01 * invS;

    const double f_upd    = f_pred    + K0 * y;
    const double fdot_upd = fdot_pred + K1 * y;

    /* covariance update: (I-KH)P */
    const double P00u = (1.0 - K0) * P00;
    const double P01u = (1.0 - K0) * P01;
    const double P11u = P11 - K1 * P01;

    /* store */
    k->f    = f_upd;
    k->fdot = fdot_upd;
    k->P00  = P00u;
    k->P01  = P01u;
    k->P11  = P11u;

    return k->f;
}

/* Helpers to retune noise on the fly */
static inline void fk_set_q(struct kalman_freq *k, double q) { k->q = q; }
static inline void fk_set_r(struct kalman_freq *k, double r) { k->r = r; }

/* Accessors */
static inline double fk_freq_hz(const struct kalman_freq *k)  { return k->f; }
static inline double fk_fdot_hzps(const struct kalman_freq *k){ return k->fdot; }

#ifdef __cplusplus
}
#endif

#endif /* FREQ_KALMAN_H */

