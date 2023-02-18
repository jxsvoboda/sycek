/*
 * Bitwise XOR
 */

char ca, cb, cc;
int a, b, c;
long la, lb, lc;
long long lla, llb, llc;

void bxor_char(void)
{
	cc = ca ^ cb;
}

void bxor(void)
{
	c = a ^ b;
}

void bxor_long(void)
{
	lc = la ^ lb;
}

void bxor_longlong(void)
{
	llc = lla ^ llb;
}
