/*
 * Bitwise OR
 */

char ca, cb, cc;
int a, b;
long la, lb, lc;
long long lla, llb, llc;

void bor_char(void)
{
	cc = ca | cb;
}

int bor(void)
{
	return a | b;
}

void bor_long(void)
{
	lc = la | lb;
}


void bor_longlong(void)
{
	llc = lla | llb;
}
