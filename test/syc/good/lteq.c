/*
 * Less than or equal
 */

int res;

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

void lteq_char(void)
{
	res = (int)(ca <= cb);
}

void lteq(void)
{
	res = (int)(a <= b);
}

void lteq_long(void)
{
	res = (int)(la <= lb);
}

void lteq_longlong(void)
{
	res = (int)(lla <= llb);
}
