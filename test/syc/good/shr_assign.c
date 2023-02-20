/*
 * Shift right assign
 */

int s;

char ca;
unsigned char uca;
int a;
unsigned ua;
long la;
unsigned long ula;
long long lla;
unsigned long long ulla;

void shr_assign_char(void)
{
	ca >>= s;
}

void shr_assign_uchar(void)
{
	uca >>= s;
}

void shr_assign(void)
{
	a >>= s;
}

void shr_assign_uint(void)
{
	ua >>= s;
}

void shr_assign_long(void)
{
	la >>= s;
}

void shr_assign_ulong(void)
{
	ula >>= s;
}

void shr_assign_longlong(void)
{
	lla >>= s;
}

void shr_assign_ulonglong(void)
{
	ulla >>= s;
}
