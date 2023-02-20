/*
 * Shift right
 */

char ca, cb;
unsigned char uca, ucb;
int a, b;
unsigned ua, ub;
long la, lb;
unsigned long ula, ulb;
long long lla, llb;
unsigned long long ulla, ullb;

int s;

void shr_char(void)
{
	ca = cb >> s;
}

void shr_uchar(void)
{
	uca = ucb >> s;
}

void shr(void)
{
	a = b >> s;
	return a;
}

void shr_uint(void)
{
	ua = ub >> s;
}

void shr_long(void)
{
	la = lb >> s;
}

void shr_ulong(void)
{
	ula = ulb >> s;
}

void shr_longlong(void)
{
	lla = llb >> s;
}

void shr_ulonglong(void)
{
	ulla = ullb >> s;
}
