/*
 * Postdecrement
 */

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

void postdec_char(void)
{
	ca = cb--;
}

void postdec(void)
{
	a = b--;
}

void postdec_long(void)
{
	la = lb--;
}

void postdec_longlong(void)
{
	lla = llb--;
}
