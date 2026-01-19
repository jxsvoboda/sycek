/*
 * Suspicious arithmetic operation involving truth values
 */

enum e {
	e1
};

int *p;
int *q;
int c;

void truthadd(void)
{
	c = (int)((0 < 1) + 1);
	c = (int)(1 + (0 < 1));
	c = (int)((0 < 1) + (0 < 1));
}

void truthaddenum(void)
{
	c = (int)(e1 + (0 < 1));
}

void truthaddptr(void)
{
	q = p + (0 < 1);
}

void truthsub(void)
{
	c = (int)((0 < 1) - 1);
	c = (int)(1 - (0 < 1));
	c = (int)((0 < 1) - (0 < 1));
}

void truthsubenum(void)
{
	c = (int)(e1 - (0 < 1));
}

void truthsubptr(void)
{
	q = p - (0 < 1);
}

void truthmul(void)
{
	c = (int)((0 < 1) * 1);
	c = (int)(1 * (0 < 1));
	c = (int)((0 < 1) * (0 < 1));
}

void truthsign(void)
{
	c = (int)-(0 < 1);
}

void truthdiv(void)
{
	c = (int)((0 < 1) / 1);
	c = (int)(1 / (0 < 1));
	c = (int)((0 < 1) / (0 < 1));
}

void truthmod(void)
{
	c = (int)((0 < 1) % 1);
	c = (int)(1 % (0 < 1));
	c = (int)((0 < 1) % (0 < 1));
}

void truthshl(void)
{
	c = (int)((0 < 1) << 1);
	c = (int)(1 << (0 < 1));
	c = (int)((0 < 1) << (0 < 1));
}

void truthshr(void)
{
	c = (int)((0 < 1) >> 1);
	c = (int)(1 >> (0 < 1));
	c = (int)((0 << 1) >> (0 < 1));
}

void truthaddassign(void)
{
	c += 0 < 1;
}

void truthsubassign(void)
{
	c -= 0 < 1;
}

void truthmulassign(void)
{
	c *= 0 < 1;
}

void truthdivassign(void)
{
	c /= 0 < 1;
}

void truthmodassign(void)
{
	c %= 0 < 1;
}

void truthshlassign(void)
{
	c <<= 0 < 1;
}

void truthshrassign(void)
{
	c >>= 0 < 1;
}
