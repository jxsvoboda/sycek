/*
 * Shift right
 */

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

int s;

void shl_char(void)
{
	ca = cb >> s;
}

int shl(void)
{
	a = b >> s;
	return a;
}

void shl_long(void)
{
	la = lb >> s;
}

void shl_longlong(void)
{
	lla = llb >> s;
}
