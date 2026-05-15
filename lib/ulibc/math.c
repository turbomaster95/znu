/*
 * math.c - Freestanding x87 FPU math library
 * 
 * Built for freestanding environments (kernels, baremetal, etc).
 * Uses raw x87 instructions with explicit stack management.
 * 
 * Compile with: -ffreestanding -nostdlib
 */

#define FORCE_FP __attribute__((target("fpmath=387")))

/* -------------------------------------------------------------------------- */
/* Forward declarations                                                       */
/* -------------------------------------------------------------------------- */

static inline double make_nan(void);
static inline int is_nan(double x);
static inline int is_inf(double x);
static inline int is_finite(double x);
static inline long long double_to_ll(double x);

static double exp2_raw(double x);

double fabs(double x);
double sqrt(double x);
double floor(double x);
double ceil(double x);
double fmod(double x, double y);
double sin(double x);
double cos(double x);
double tan(double x);
double atan(double x);
double atan2(double y, double x);
double log(double x);
double log10(double x);
double ldexp(double value, int exp);
double exp(double x);
double pow(double x, double y);
double frexp(double value, int *eptr);
double modf(double value, double *iptr);
double asin(double x);
double acos(double x);

/* -------------------------------------------------------------------------- */
/* Compiler / Freestanding Helpers                                            */
/* -------------------------------------------------------------------------- */

static inline double make_nan(void) {
    union { double d; unsigned long long u; } u;
    u.u = 0x7FF8000000000000ULL;
    return u.d;
}

static inline int is_nan(double x) {
    union { double d; unsigned long long u; } u = { x };
    return ((u.u & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL) &&
           ((u.u & 0x000FFFFFFFFFFFFFULL) != 0);
}

static inline int is_inf(double x) {
    union { double d; unsigned long long u; } u = { x };
    return ((u.u & 0x7FFFFFFFFFFFFFFFULL) == 0x7FF0000000000000ULL);
}

static inline int is_finite(double x) {
    union { double d; unsigned long long u; } u = { x };
    return (u.u & 0x7FF0000000000000ULL) != 0x7FF0000000000000ULL;
}

static inline long long double_to_ll(double x) {
    long long res;
    __asm__ volatile (
        "fldl %1\n\t"
        "fistpll %0"
        : "=m" (res)
        : "m" (x)
        : "memory"
    );
    return res;
}

/* -------------------------------------------------------------------------- */
/* Basic Functions                                                            */
/* -------------------------------------------------------------------------- */

//FORCE_FP double fabs(double x) {
//    double res;
//    __asm__ volatile (
//        "fldl %1\n\t"
//        "fabs\n\t"
//        "fstpl %0"
//        : "=m" (res)
//        : "m" (x)
//        : "memory"
//    );
//    return res;
//}

FORCE_FP double sqrt(double x) {
    double res;
    __asm__ volatile (
        "fldl %1\n\t"
        "fsqrt\n\t"
        "fstpl %0"
        : "=m" (res)
        : "m" (x)
        : "memory"
    );
    return res;
}

FORCE_FP double floor(double x) {
    if (!is_finite(x)) return x;
    long long i = double_to_ll(x);
    double result = (double)i;
    if (x >= 0) return result;
    return (x == result) ? result : result - 1.0;
}

FORCE_FP double ceil(double x) {
    if (!is_finite(x)) return x;
    long long i = double_to_ll(x);
    double result = (double)i;
    if (x <= 0) return result;
    return (x == result) ? result : result + 1.0;
}

FORCE_FP double fmod(double x, double y) {
    double res;
    unsigned short sw;
    if (y == 0.0) return make_nan();
    __asm__ volatile (
        "fldl %2\n\t"
        "fldl %1\n\t"
        "1: fprem\n\t"
        "fstsw %0\n\t"
        "andw $0x0400, %0\n\t"
        "jnz 1b\n\t"
        "fstpl %3\n\t"
        "ffreep %%st(0)"
        : "=&r" (sw), "=m" (res)
        : "m" (x), "m" (y)
        : "memory", "cc"
    );
    return res;
}

/* -------------------------------------------------------------------------- */
/* Trigonometry                                                               */
/* -------------------------------------------------------------------------- */

FORCE_FP double sin(double x) {
    double res;
    __asm__ volatile (
        "fldl %1\n\t"
        "fsin\n\t"
        "fstpl %0"
        : "=m" (res)
        : "m" (x)
        : "memory"
    );
    return res;
}

FORCE_FP double cos(double x) {
    double res;
    __asm__ volatile (
        "fldl %1\n\t"
        "fcos\n\t"
        "fstpl %0"
        : "=m" (res)
        : "m" (x)
        : "memory"
    );
    return res;
}

FORCE_FP double tan(double x) {
    double res, dummy;
    __asm__ volatile (
        "fldl %2\n\t"
        "fptan\n\t"
        "fstpl %0\n\t"
        "fstpl %1"
        : "=m" (dummy), "=m" (res)
        : "m" (x)
        : "memory"
    );
    return res;
}

FORCE_FP double atan(double x) {
    double res;
    __asm__ volatile (
        "fldl %1\n\t"
        "fld1\n\t"
        "fpatan\n\t"
        "fstpl %0"
        : "=m" (res)
        : "m" (x)
        : "memory"
    );
    return res;
}

FORCE_FP double atan2(double y, double x) {
    double res;
    __asm__ volatile (
        "fldl %2\n\t"
        "fldl %1\n\t"
        "fpatan\n\t"
        "fstpl %0"
        : "=m" (res)
        : "m" (y), "m" (x)
        : "memory"
    );
    return res;
}

/* -------------------------------------------------------------------------- */
/* Exponential and Logarithmic                                               */
/* -------------------------------------------------------------------------- */

FORCE_FP double log(double x) {
    double res;
    if (x <= 0.0) return make_nan();
    __asm__ volatile (
        "fldln2\n\t"
        "fldl %1\n\t"
        "fyl2x\n\t"
        "fstpl %0"
        : "=m" (res)
        : "m" (x)
        : "memory"
    );
    return res;
}

FORCE_FP double log10(double x) {
    double res;
    if (x <= 0.0) return make_nan();
    __asm__ volatile (
        "fldlg2\n\t"
        "fldl %1\n\t"
        "fyl2x\n\t"
        "fstpl %0"
        : "=m" (res)
        : "m" (x)
        : "memory"
    );
    return res;
}

FORCE_FP double ldexp(double value, int exp) {
    double res;
    __asm__ volatile (
        "fildl %2\n\t"
        "fldl %1\n\t"
        "fscale\n\t"
        "fstpl %0\n\t"
        "ffreep %%st(0)"
        : "=m" (res)
        : "m" (value), "m" (exp)
        : "memory"
    );
    return res;
}

static double exp2_raw(double x) {
    double res;
    double intpart = floor(x + 0.5);
    double frac = x - intpart;
    __asm__ volatile (
        "fldl %1\n\t"
        "f2xm1\n\t"
        "fld1\n\t"
        "faddp\n\t"
        "fstpl %0"
        : "=m" (res)
        : "m" (frac)
        : "memory"
    );
    return ldexp(res, (int)intpart);
}

static const double LN2_INV = 1.44269504088896340736;

FORCE_FP double exp(double x) {
    return exp2_raw(x * LN2_INV);
}

FORCE_FP double pow(double x, double y) {
    if (x == 0.0) {
        if (y > 0.0) return 0.0;
        if (y < 0.0) return make_nan();
        return 1.0;
    }
    if (y == 0.0) return 1.0;
    if (x < 0.0) {
        double intpart;
        modf(y, &intpart);
        if (y == intpart) {
            double res = exp2_raw(y * log(-x) * LN2_INV);
            return ((long long)y & 1) ? -res : res;
        }
        return make_nan();
    }
    return exp2_raw(y * log(x) * LN2_INV);
}

/* -------------------------------------------------------------------------- */
/* Deconstruction Helpers                                                     */
/* -------------------------------------------------------------------------- */

FORCE_FP double frexp(double value, int *eptr) {
    union { double d; unsigned long long u; } u = { value };
    int ee = (int)((u.u >> 52) & 0x7FF);
    if (!ee) {
        if (value != 0.0) {
            u.d = value * 9007199254740992.0;
            ee = (int)((u.u >> 52) & 0x7FF) - 53;
        } else {
            *eptr = 0;
            return value;
        }
    } else if (ee == 0x7FF) {
        *eptr = 0;
        return value;
    }
    *eptr = ee - 1022;
    u.u &= 0x800FFFFFFFFFFFFFULL;
    u.u |= 0x3FE0000000000000ULL;
    return u.d;
}

FORCE_FP double modf(double value, double *iptr) {
    if (value >= 0.0) {
        *iptr = floor(value);
    } else {
        *iptr = ceil(value);
    }
    return value - *iptr;
}

FORCE_FP double asin(double x) {
    return atan2(x, sqrt(1.0 - x * x));
}

FORCE_FP double acos(double x) {
    return atan2(sqrt(1.0 - x * x), x);
}

