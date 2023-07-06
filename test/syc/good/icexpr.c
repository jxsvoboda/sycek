/*
 * Integer constant expression.
 */

enum {
	/* Integer literal */
	e1 = 1,
	/* Enum element */
	e2 = e1,
	/* Add int + int */
	e3 = 10 + 1,
	/* Add enum + int */
	e4 = e2 + 1
};

int z;

void se1(void)
{
	z = e1;
}

void se2(void)
{
	z = e2;
}

void se3(void)
{
	z = e3;
}

void se4(void)
{
	z = e4;
}
