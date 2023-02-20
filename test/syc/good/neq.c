/*
 * Not equal
 */

int res;

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

void neq_char(void)
{
	res = (int)(ca != cb);
}

void neq(void)
{
	res = (int)(a != b);
}

void neq_long(void)
{
	res = (int)(la != lb);
}

void neq_longlong(void)
{
	res = (int)(lla != llb);
}
