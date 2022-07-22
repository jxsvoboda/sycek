/*
 * Less than
 */

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

int lt_char(void)
{
	return (int)(ca < cb);
}

int lt(void)
{
	return (int)(a < b);
}

int lt_long(void)
{
	return (int)(la < lb);
}

int lt_longlong(void)
{
	return (int)(lla < llb);
}
