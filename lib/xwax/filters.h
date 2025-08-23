#ifndef FILTERS_H

#define FILTERS_H

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

#endif /* end of include guard FILTERS_H */
