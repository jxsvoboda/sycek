/*
 * Unsigned comparison of mixed-sign integers
 *
 * This warning is produced when comparing a signed and unsigned integer
 * and usual arithmetic conversion first converts them to unsigned type.
 * This occurs in all cases except when the unsigned type can be fully
 * represented in the signed type, i.e. when it is shorter. For example,
 * comparing and unsigned char with int is OK, because both operands
 * are converted to signed int (and thus no warning is produced).
 */

int res;

char c;
unsigned char uc;
int i;
unsigned u;
long l;
unsigned long ul;
long long ll;
unsigned long long ull;

void cmp_char_uchar(void)
{
	res = (int)(c < uc);
}

void cmp_char_uint(void)
{
	res = (int)(c < u);
}

void cmp_char_ulong(void)
{
	res = (int)(c < ul);
}

void cmp_char_ulonglong(void)
{
	res = (int)(c < ull);
}

void cmp_int_uint(void)
{
	res = (int)(i < u);
}

void cmp_int_ulong(void)
{
	res = (int)(i < ul);
}

void cmp_int_ulonglong(void)
{
	res = (int)(i < ull);
}

void cmp_long_ulong(void)
{
	res = (int)(l < ul);
}

void cmp_long_ulonglong(void)
{
	res = (int)(l < ull);
}

void cmp_longlong_ulonglong(void)
{
	res = (int)(ll < ull);
}
