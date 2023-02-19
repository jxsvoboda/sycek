/*
 * Greater than or equal
 */

int res;

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

void gteq_char(void)
{
	res = (int)(ca >= cb);
}

void gteq(void)
{
	res = (int)(a >= b);
}

void gteq_long(void)
{
	res = (int)(la >= lb);
}

void gteq_longlong(void)
{
	res = (int)(lla >= llb);
}
