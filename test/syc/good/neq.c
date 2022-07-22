/*
 * Not equal
 */

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

int neq_char(void)
{
	return (int)(ca != cb);
}

int neq(void)
{
	return (int)(a != b);
}

int neq_long(void)
{
	return (int)(la != lb);
}

int neq_longlong(void)
{
	return (int)(lla != llb);
}
