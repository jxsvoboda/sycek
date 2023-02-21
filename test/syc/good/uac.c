/*
 * Usual arithmetic conversion
 *
 * Usual arithmetic conversion is performed when most binary operands
 * are used with arguments of different integral types. We test here
 * with addition.
 */

char ca, cb;
unsigned char uca, ucb;
int a, b;
unsigned ua, ub;
long la, lb;
unsigned long ula, ulb;
long long lla, llb;
unsigned long long ulla, ullb;

void add_char_int(void)
{
	a = cb + b;
}

void add_char_uint(void)
{
	ua = cb + ub;
}

void add_uchar_int(void)
{
	a = ucb + b;
}

void add_uchar_uint(void)
{
	ua = ucb + ub;
}

void add_char_long(void)
{
	la = cb + lb;
}

void add_char_ulong(void)
{
	ula = cb + ulb;
}

void add_uchar_long(void)
{
	la = ucb + lb;
}

void add_uchar_ulong(void)
{
	ula = ucb + ulb;
}

void add_char_longlong(void)
{
	lla = cb + llb;
}

void add_char_ulonglong(void)
{
	ulla = cb + ullb;
}

void add_uchar_longlong(void)
{
	lla = ucb + llb;
}

void add_uchar_ulonglong(void)
{
	ulla = ucb + ullb;
}

void add_int_long(void)
{
	la = b + lb;
}

void add_int_ulong(void)
{
	ula = b + ulb;
}

void add_uint_long(void)
{
	la = ub + lb;
}

void add_uint_ulong(void)
{
	ula = ub + ulb;
}

void add_int_longlong(void)
{
	lla = b + llb;
}

void add_int_ulonglong(void)
{
	ulla = b + ullb;
}

void add_uint_longlong(void)
{
	lla = ub + llb;
}

void add_uint_ulonglong(void)
{
	ulla = ub + ullb;
}

void add_long_longlong(void)
{
	lla = lb + llb;
}

void add_long_ulonglong(void)
{
	ulla = lb + ullb;
}

void add_ulong_longlong(void)
{
	lla = ulb + llb;
}

void add_ulong_ulonglong(void)
{
	ulla = ulb + ullb;
}
