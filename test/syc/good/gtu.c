/*
 * Greater than unsigned
 */

int res;

unsigned char ca, cb;
unsigned a, b;
unsigned long la, lb;
unsigned long long lla, llb;

void gtu_char(void)
{
	res = (int)(ca > cb);
}

void gtu(void)
{
	res = (int)(a > b);
}

void gtu_long(void)
{
	res = (int)(la > lb);
}

void gtu_longlong(void)
{
	res = (int)(lla > llb);
}
