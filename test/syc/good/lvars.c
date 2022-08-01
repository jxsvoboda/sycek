/*
 * Local variables
 */

void lvars_char(void)
{
	char ca, cb;

//	ca = '1';
//	cb = '2';
	ca += cb;
}

int lvars(void)
{
	int a, b;

	a = 1;
	b = 2;
	a += b;

	return a;
}

void lvars_long(void)
{
	long la, lb;

	la = 1l;
	lb = 2l;
	la += lb;
}

void lvars_longlong(void)
{
	long long lla, llb;

	lla = 1ll;
	llb = 2ll;
	lla += llb;
}
