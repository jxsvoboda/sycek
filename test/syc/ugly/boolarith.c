/*
 * Suspicious arithmetic operation involving truth values (bool)
 */

enum e {
	e1
};

_Bool a, b, c;
int i;
int *p;
int *q;

void booladd(void)
{
	c = a + b;
	i = a + 1;
	i = 1 + a;
}

void booladdenum(void)
{
	c = (_Bool)(e1 + a);
}

void booladdptr(void)
{
	q = p + a;
}

void boolsub(void)
{
	i = a - 1;
	i = 1 - a;
	c = a - b;
}

void boolsubenum(void)
{
	c = (_Bool)(e1 - a);
	c = (_Bool)(a - e1);
}

void boolsubptr(void)
{
	q = p - a;
}

void boolmul(void)
{
	c = a * b;
	c = (_Bool)(1 * a);
	c = (_Bool)(a * 1);
}

void boolsign(void)
{
	c = (_Bool)-a;
}

void booldiv(void)
{
	i = 1 / a;
	i = a / 1;
	c = a / b;
}

void boolmod(void)
{
	i = 1 % a;
	i = a % 1;
	c = a % b;
}

void boolhinc(void)
{
	c++;
	++c;
}

void booldec(void)
{
	c--;
	--c;
}

void boolshl(void)
{
	c = a << 1;
	i = 1 << a;
	c = a << b;
}

void boolshr(void)
{
	c = a >> 1;
	i = 1 >> a;
	c = a >> b;
}

void booladdassign(void)
{
	i += a;
	c += 1;
	c += a;
}

void boolsubassign(void)
{
	i -= a;
	c -= 1;
	c -= a;
}

void boolmulassign(void)
{
	i *= a;
	c *= 1;
	c *= a;
}

void booldivassign(void)
{
	i /= a;
	c /= 1;
	c /= a;
}

void boolmodassign(void)
{
	i %= a;
	c %= 1;
	c %= a;
}

void boolshlassign(void)
{
	i <<= a;
	c <<= 1;
	c <<= a;
}

void boolshrassign(void)
{
	i >>= a;
	c >>= 1;
	c >>= a;
}
