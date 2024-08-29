/*
 * Unsigned division and modulus
 */

unsigned char ca, cb, cq, cr;
unsigned a, b, q, r;
unsigned long la, lb, lq, lr;
unsigned long long lla, llb, llq, llr;

void div_char(void)
{
	cq = ca / cb;
}

void mod_char(void)
{
	cr = ca % cb;
}

void div(void)
{
	q = a / b;
}

void mod(void)
{
	r = a % b;
}

void div_long(void)
{
	lq = la / lb;
}

void mod_long(void)
{
	lr = la % lb;
}

void div_longlong(void)
{
	llq = lla / llb;
}

void mod_longlong(void)
{
	llr = lla % llb;
}
