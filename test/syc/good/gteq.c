/*
 * Greater than or equal
 */

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

int gteq_char(void)
{
	return (int)(ca >= cb);
}

int gteq(void)
{
	return (int)(a >= b);
}

int gteq_long(void)
{
	return (int)(la >= lb);
}

int gteq_longlong(void)
{
	return (int)(lla >= llb);
}
