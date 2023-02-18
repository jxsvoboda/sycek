/*
 * Equal
 */
int e;

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

void eq_char(void)
{
	e = (int)(ca == cb);
}

void eq(void)
{
	e = (int)(a == b);
}

void eq_long(void)
{
	e = (int)(la == lb);
}

void eq_longlong(void)
{
	e = (int)(lla == llb);
}
