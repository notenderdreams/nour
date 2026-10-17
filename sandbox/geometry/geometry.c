#include "geometry.h"

#include <math.h>

double geo_circle_area(double radius) { return M_PI * radius * radius; }
double geo_hypotenuse(double a, double b) { return sqrt(a * a + b * b); }
