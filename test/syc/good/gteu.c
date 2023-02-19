/*
 * Greater than or equal unsigned
 */

int res;

unsigned char ca, cb;
unsigned a, b;
unsigned long la, lb;
unsigned long long lla, llb;

void gteu_char(void)
{
	res = (int)(ca >= cb);
}

void gteu(void)
{
	res = (int)(a >= b);
}

void gteu_long(void)
{
	res = (int)(la >= lb);
}

void gteu_longlong(void)
{
	res = (int)(lla >= llb);
}
