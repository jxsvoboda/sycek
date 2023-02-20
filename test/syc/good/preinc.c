/*
 * Preincrement
 */

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

void preinc_char(void)
{
	ca = ++cb;
}

void preinc(void)
{
	a = ++b;
}

void preinc_long(void)
{
	la = ++lb;
}

void preinc_longlong(void)
{
	lla = ++llb;
}
