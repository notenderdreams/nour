#include <stdio.h>
#include "math_ops.h"

int main(void)
{
	int x = 20;
	int y = 22;

	printf("Computing in C:\n");
	printf("  %d + %d = %d\n", x, y, add(x, y));
	printf("  %d * %d = %d\n", x, y, multiply(x, y));

	return 0;
}
