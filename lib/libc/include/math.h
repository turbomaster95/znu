#ifndef _MATH_H
#define _MATH_H

#define NAN (0.0f / 0.0f)
#define INFINITY (1.0f / 0.0f)

#define abs(x)  ((x) < 0 ? -(x) : (x))
#define fabs(x) ((x) < 0 ? -(x) : (x))

//double pow(double base, double exp);
//double ldexp(double x, int exp);

static inline double floor(double x) {
    int i = (int)x;
    return (double)(x < i ? i - 1 : i);
}

#endif /* _MATH_H */
