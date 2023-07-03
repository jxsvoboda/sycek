/*
 * Integer constant expression.
 */

enum {
	/* Integer literal */
	e1 = 1,
	/* Enum element */
	e2 = e1
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
