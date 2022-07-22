/*
 * Equal
 */

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

int eq_char(void)
{
	return (int)(ca == cb);
}

int eq(void)
{
	return (int)(a == b);
}

int eq_long(void)
{
	return (int)(la == lb);
}

int eq_longlong(void)
{
	return (int)(lla == llb);
}
