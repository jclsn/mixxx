
/*
 * Kalman filter replacement for alpha-beta pitch estimator
 * Model: constant velocity (x,v) with acceleration process noise
 *
 * State:
 *   x ... displacement over the last interval (kept relative by subtracting dx at the end)
 *   v ... velocity (pitch)
 *
 * Observation:
 *   z = dx  (measured displacement over dt)
 */

#ifndef PITCH_H
#define PITCH_H

#include <math.h>
#include <string.h> /* for memset */

struct pitch {
    /* configuration */
    double dt;     /* sample period (s) */
    double q;      /* process noise spectral density (acceleration), e.g. (rad/s^2)^2 * s */
    double r;      /* measurement noise variance for dx (units^2) */

    /* state */
    double x;      /* position (relative; we subtract dx each step) */
    double v;      /* velocity (pitch) */

    /* covariance P (2x2, symmetric) */
    double Pxx, Pxv, Pvv;
};

/* Initialize with dt and noise params.
   q: acceleration noise spectral density (tune up if motion is more jittery)
   r: variance of dx measurement (tune up if observations are noisier) */
static inline void pitch_init(struct pitch *p, double dt, double q, double r)
{
    p->dt = dt;
    p->q  = q;
    p->r  = r;

    p->x = 0.0;
    p->v = 0.0;

    /* Start with large uncertainty so the filter trusts early measurements */
    p->Pxx = 1e6;
    p->Pxv = 0.0;
    p->Pvv = 1e6;
}

/* Optionally retune noise without resetting state */
static inline void pitch_set_noise(struct pitch *p, double q, double r)
{
    p->q = q;
    p->r = r;
}

/* Optionally change dt (if your cadence changes) */
static inline void pitch_set_dt(struct pitch *p, double dt)
{
    p->dt = dt;
}

/* Feed one observation: in the last dt seconds, position moved by dx */
static inline void pitch_dt_observation(struct pitch *p, double dx)
{
    const double dt = p->dt;

    /* ==== Predict step ====
       State transition F = [1 dt; 0 1]
       Process noise for constant-velocity model with white accel (G*q*G^T):
         Q = q * [dt^3/3  dt^2/2;
                  dt^2/2  dt]
    */
    const double F11 = 1.0, F12 = dt;
    const double F21 = 0.0, F22 = 1.0;

    const double dt2 = dt*dt;
    const double dt3 = dt2*dt;
    const double q11 = p->q * (dt3 / 3.0);
    const double q12 = p->q * (dt2 / 2.0);
    const double q22 = p->q * (dt);

    /* Predict state */
    double x_pred = p->x + p->v * dt;
    double v_pred = p->v;

    /* Predict covariance: P = F P F^T + Q
       Compute with minimal temporaries and keep symmetry */
    double Pxx = p->Pxx, Pxv = p->Pxv, Pvv = p->Pvv;

    double Pxx_pred = F11*(F11*Pxx + F12*Pxv) + F12*(F11*Pxv + F12*Pvv) + q11;
    double Pxv_pred = F21*(F11*Pxx + F12*Pxv) + F22*(F11*Pxv + F12*Pvv) + q12;
    double Pvv_pred = F21*(F21*Pxx + F22*Pxv) + F22*(F21*Pxv + F22*Pvv) + q22;

    /* ==== Update step ====
       Measurement z = H x + noise, with H = [1 0], R = r
       Innovation: y = z - H x_pred = dx - x_pred
    */
    const double Hx = 1.0, Hv = 0.0;
    double y = dx - (Hx * x_pred + Hv * v_pred);

    /*
     * Mode switch for stable playback and scratching
     */

    double residual_var = fabs(y); // or y*y
    double threshold = 8e-4;

    if (residual_var > threshold) {
        p->r = 1e-4;  // more reactive
        p->q = 1e-1;  // more reactive
    } else {
        p->r = 10.0;  // more stable
        p->q = 1e-7;  // more stable
    }

    /* Innovation covariance: S = H P H^T + R = Pxx_pred + r */
    double S = Pxx_pred + p->r;

    /* Kalman gain: K = P H^T S^-1 = [Kx; Kv] */
    double invS = 1.0 / S;
    double Kx = Pxx_pred * invS;
    double Kv = Pxv_pred * invS;

    /* Update state */
    double x_upd = x_pred + Kx * y;
    double v_upd = v_pred + Kv * y;

    /* Update covariance: P = (I - K H) P
       With H = [1 0], (I-KH) = [[1-Kx,   0],
                                 [-Kv,    1]]
       Compute symmetric entries explicitly
    */
    double Pxx_upd = (1.0 - Kx) * Pxx_pred;
    double Pxv_upd = (1.0 - Kx) * Pxv_pred;       /* equals Pvx_upd */
    double Pvv_upd = Pvv_pred - Kv * Pxv_pred;

    /* Keep the same “relative position” convention as your α-β code */
    x_upd -= dx;

    /* Store back */
    p->x = x_upd;
    p->v = v_upd;
    p->Pxx = Pxx_upd;
    p->Pxv = Pxv_upd;
    p->Pvv = Pvv_upd;
}

/* Get the current pitch (velocity estimate) */
static inline double pitch_current(const struct pitch *p)
{
    return p->v;
}

#endif /* PITCH_H */
