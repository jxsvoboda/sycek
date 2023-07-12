/*
 * Integer constant expression - logical operators
 */

enum {
	/* false && false */
	e1 = (int)(0 < 0 && 0 < 0),
	/* false && true */
	e2 = (int)(0 < 0 && 0 < 1),
	/* true && false */
	e3 = (int)(0 < 1 && 0 < 0),
	/* true && true */
	e4 = (int)(0 < 1 && 0 < 1),
	/* false || false */
	e5 = (int)(0 < 0 || 0 < 0),
	/* false || true */
	e6 = (int)(0 < 0 || 0 < 1),
	/* true || false */
	e7 = (int)(0 < 1 || 0 < 0),
	/* true || true */
	e8 = (int)(0 < 1 || 0 < 1),
	/* !false */
	e9 = (int)!(0 < 0),
	/* !true */
	e10 = (int)!(0 < 1)
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
