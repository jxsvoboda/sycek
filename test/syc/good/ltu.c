/*
 * Less than unsigned
 */

int res;

unsigned char ca, cb;
unsigned a, b;
unsigned long la, lb;
unsigned long long lla, llb;

void ltu_char(void)
{
	res = (int)(ca < cb);
}

void ltu(void)
{
	res = (int)(a < b);
}

void ltu_long(void)
{
	res = (int)(la < lb);
}

void ltu_longlong(void)
{
	res = (int)(lla < llb);
}
