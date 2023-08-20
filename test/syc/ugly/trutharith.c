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

/* TODO / */
/* TODO % */
/* TODO ++ once we have _Bool */
/* TODO -- once we have _Bool */

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

/* TODO Left operand of compound assignment is _Bool */

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

/* TODO /= */
/* TODO %= */

void truthshlassign(void)
{
	c <<= 0 < 1;
}

void truthshrassign(void)
{
	c >>= 0 < 1;
}
