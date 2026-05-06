#include <math.h>

//double ldexp(double x, int exp) {
//    if (exp > 0) {
//        while (exp--) x *= 2.0;
//    } else if (exp < 0) {
//        while (exp++) x /= 2.0;
//    }
//    return x;
//}

//double pow(double base, double exp) {
//    if (exp == 0) return 1;
//    if (base == 0) return 0;
//    
//    double res = 1;
//    int e = (int)exp; 
//    for (int i = 0; i < e; i++) res *= base;
//    return res;
//}
