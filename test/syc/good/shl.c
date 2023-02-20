/*
 * Shift left
 */

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

int s;

void shl_char(void)
{
	ca = cb << s;
}

void shl(void)
{
	a = b << s;
}

void shl_long(void)
{
	la = lb << s;
}

void shl_longlong(void)
{
	lla = llb << s;
}
