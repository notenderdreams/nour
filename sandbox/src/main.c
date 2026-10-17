// Transitive public defines must arrive from mathlib (via .deps):
#ifndef MATH_LIB
#error "public_define MATH_LIB did not propagate from mathlib"
#endif

#include <stdio.h>

#include "advanced_ops.h"
#include "geometry.h"
#include "math_ops.h"
#include "strutil.h"

int main(void)
{
	char buf[64] = {0};
	str_upper(buf, sizeof(buf), "nour dependency resolution works");

	printf("C build system with dependencies:\n");
	printf("  20 + 22        = %d\n", add(20, 22));
	printf("  2 * 21         = %d\n", multiply(2, 21));
	printf("  2 ^ 8          = %d\n", power(2, 8));
	printf("  area(2.0)      = %g\n", geo_circle_area(2.0));
	printf("  hyp(3.0, 4.0)  = %g\n", geo_hypotenuse(3.0, 4.0));
	printf("  lowered text   = %s\n", buf);

	int ok = add(20, 22) == 42 && multiply(2, 21) == 42 && power(2, 8) == 256 &&
		 geo_hypotenuse(3.0, 4.0) == 5.0 &&
		 (geo_circle_area(2.0) > 12.5 && geo_circle_area(2.0) < 12.6);

	return ok ? 0 : 1;
}
