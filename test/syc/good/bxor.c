/*
 * Bitwise XOR
 */

char ca, cb, cc;
int a, b;
long la, lb, lc;
long long lla, llb, llc;

void bxor_char(void)
{
	cc = ca ^ cb;
}

int bxor(void)
{
	return a ^ b;
}

void bxor_long(void)
{
	lc = la ^ lb;
}


void bxor_longlong(void)
{
	llc = lla ^ llb;
}
