#ifndef FILTERS_H

#define FILTERS_H

#define ORD(x) ((sizeof(x) / sizeof(*x)-1))

struct ema_filter {
    double alpha;
    int y_old;
};

void ema_init(struct ema_filter *, const double alpha);
int ema(struct ema_filter *, const int x);

struct emaf_filter {
    double alpha;
    double y_old;
};

int emaf(struct emaf_filter *filter, const double x);
void emaf_init(struct emaf_filter *, const double alpha);

struct differentiator {
    int x_old;
};

void derivative_init(struct differentiator *filter);
int derivative(struct differentiator *, const int x);

struct root_mean_square {
    float alpha;
    unsigned long long squared_old;
};

void rms_init(struct root_mean_square *filter, const float alpha);
int rms(struct root_mean_square *filter, const int x);

struct apbp_filter {
    double c;
    double d;
    int xh[2];
};

void apbp_init(struct apbp_filter *filter, double Fc, double Fb, unsigned int sample_rate);
int apbp(struct apbp_filter *filter, int x);


// Example: 4th-order Butterworth
struct butterworth_filter {
    double b[5]; // numerator coefficients
    double a[5]; // denominator coefficients
    double x[5]; // past input samples
    double y[5]; // past output samples
};

void butterworth_init(struct butterworth_filter* f, const double b[5], const double a[5]);
double butterworth(struct butterworth_filter* f, double xn);

struct iir_filter {
    int ord;
    double *b;
    double *a;
    double *x; // past inputs
    double *y; // past outputs
};

struct iir_filter* iir_init(int ord, const double* b, const double* a);
double iir_filter(struct iir_filter* f, double xn);
void iir_free(struct iir_filter* f);

#endif /* end of include guard FILTERS_H */
