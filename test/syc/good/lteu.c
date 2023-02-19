/*
 * Less than or equal unsigned
 */

int res;

unsigned char ca, cb;
unsigned a, b;
unsigned long la, lb;
unsigned long long lla, llb;

void lteu_char(void)
{
	res = (int)(ca <= cb);
}

void lteu(void)
{
	res = (int)(a <= b);
}

void lteu_long(void)
{
	res = (int)(la <= lb);
}

void lteu_longlong(void)
{
	res = (int)(lla <= llb);
}
