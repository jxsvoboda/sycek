/*
 * Integer constant expression - comparison operators
 */

enum {
	e1 = (int)(1 < 2),
	e2 = (int)(2 < 1),
	e3 = (int)(1 <= 2),
	e4 = (int)(2 <= 1),
	e5 = (int)(1 > 2),
	e6 = (int)(2 > 1),
	e7 = (int)(1 == 2),
	e8 = (int)(1 == 1),
	e9 = (int)(1 != 2),
	e10 = (int)(1 != 1)
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

void se9(void)
{
	z = e9;
}

void se10(void)
{
	z = e10;
}
