/*
 * Less than
 */

int res;

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

void lt_char(void)
{
	res = (int)(ca < cb);
}

void lt(void)
{
	res = (int)(a < b);
}

void lt_long(void)
{
	res = (int)(la < lb);
}

void lt_longlong(void)
{
	res = (int)(lla < llb);
}
