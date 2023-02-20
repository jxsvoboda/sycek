/*
 * Postincrement
 */

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

void postinc_char(void)
{
	ca = cb++;
}

void postinc(void)
{
	a = b++;
}

void postinc_long(void)
{
	la = lb++;
}

void postinc_longlong(void)
{
	lla = llb++;
}
