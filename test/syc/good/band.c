/*
 * Bitwise AND
 */

char ca, cb, cc;
int a, b, c;
long la, lb, lc;
long long lla, llb, llc;

void band_char(void)
{
	cc = ca & cb;
}

void band(void)
{
	c = a & b;
}

void band_long(void)
{
	lc = la & lb;
}

void band_longlong(void)
{
	llc = lla & llb;
}
