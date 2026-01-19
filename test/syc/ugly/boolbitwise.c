/*
 * Suspicious bitwise operation involving truth values (bool)
 */

int i;
_Bool a, b, c;

void boolband(void)
{
	i = a & 1;
	i = 1 & a;
	c = a & b;
}

void boolbor(void)
{
	i = a | 1;
	i = 1 | a;
	c = a | b;
}

void boolbxor(void)
{
	i = a ^ 1;
	i = 1 ^ a;
	c = a ^ b;
}

void boolbnot(void)
{
	c = ~a;
}

void boolborassign(void)
{
	c |= a;
}

void boolbandassign(void)
{
	c &= a;
}

void boolbxorassign(void)
{
	c ^= a;
}
