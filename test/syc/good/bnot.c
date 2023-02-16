/*
 * Bitwise NOT
 */

char ca, cb;
int a, b;
long la, lb;
long long lla, llb;

void bnot_char(void)
{
	cb = ~ca;
}

void bnot(void)
{
	b = ~a;
}

void bnot_long(void)
{
	lb = ~la;
}

void bnot_longlong(void)
{
	llb = ~lla;
}
