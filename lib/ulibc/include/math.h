#ifndef _MATH_H
#define _MATH_H

#define HUGE_VAL (__builtin_huge_val())
#define NAN      (__builtin_nan(""))

double floor(double x);
double ceil(double x);
double fabs(double x);
double fmod(double x, double y);
double frexp(double value, int *eptr);
double ldexp(double value, int exp);
double modf(double value, double *iptr);
double sqrt(double x);

double pow(double x, double y);
double log(double x);
double log10(double x);
double exp(double x);

double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);

#endif /* _MATH_H */
