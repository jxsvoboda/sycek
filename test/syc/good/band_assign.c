/*
 * Bitwise AND assign
 */

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

void band_assign_char(void)
{
	ca &= cb;
}

int band_assign(void)
{
	a &= b;
}

void band_assign_long(void)
{
	la &= lb;
}

void band_assign_longlong(void)
{
	lla &= llb;
}
