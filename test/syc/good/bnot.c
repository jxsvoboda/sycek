/*
 * Bitwise NOT
 */

char ca, cb;
int a;
long la, lb;
long long lla, llb;

void bnot_char(void)
{
	cb = ~ca;
}

int bnot(void)
{
	return ~a;
}

void bnot_long(void)
{
	lb = ~la;
}

void bnot_longlong(void)
{
	llb = ~lla;
}
