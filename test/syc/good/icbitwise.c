/*
 * Integer constant expression - bitwise operators
 */

enum {
	/* Bitwise AND */
	e1 = 0x0f & 0x77,
	/* Bitwise OR */
	e2 = 0x0f | 0x77,
	/* Bitwise XOR */
	e3 = 0x0f ^ 0x77,
	/* Bitwise NOT */
	e4 = ~0x0f,

	v1 = 0x0f,
	v2 = 0x77,

	/* Bitwise AND enums */
	e5 = v1 & v2,
	/* Bitwise OR enums */
	e6 = v1 | v2,
	/* Bitwise XOR enums */
	e7 = v1 ^ v2,
	/* Bitwise NOT enum */
	e8 = ~v1
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

void se5(void)
{
	z = e5;
}

void se6(void)
{
	z = e6;
}

void se7(void)
{
	z = e7;
}

void se8(void)
{
	z = e8;
}
