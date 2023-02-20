/*
 * Equal
 */
int res;

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

void eq_char(void)
{
	res = (int)(ca == cb);
}

void eq(void)
{
	res = (int)(a == b);
}

void eq_long(void)
{
	res = (int)(la == lb);
}

void eq_longlong(void)
{
	res = (int)(lla == llb);
}
