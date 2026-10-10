#include <stdio.h>
#include "advanced_ops.h"
#include "math_ops.h"

int main(void)
{
	int x = 20;
	int y = 22;

	printf("Computing in C:\n");
	printf("  %d + %d = %d\n", x, y, add(x, y));
	printf("  %d * %d = %d\n", x, y, multiply(x, y));
	printf("  2 ^ 8 = %d\n", power(2, 8));

	return 0;
}
