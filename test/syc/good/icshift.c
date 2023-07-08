/*
 * Integer constant expression - shift operators
 */

enum {
	/* Shift left */
	e1 = 10 << 1,
	/* Shift right */
	e2 = 10 >> 1,
	/* Shift negative value right */
	e3 = (-10) >> 1
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
