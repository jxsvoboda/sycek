/*
 * Less than or equal
 */

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

int lteq_char(void)
{
	return (int)(ca <= cb);
}

int lteq(void)
{
	return (int)(a <= b);
}

int lteq_long(void)
{
	return (int)(la <= lb);
}

int lteq_longlong(void)
{
	return (int)(lla <= llb);
}
