/*
 * Comparison of mixed-sign integers
 *
 * When comparing a signed and unsigned integer and usual arithmetic
 * conversion first converts them to signed type, no warning is produced.
 * This occurs when the unsigned type can be fully represented in the signed
 * type, i.e. when it is shorter. For example,
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

void cmp_uchar_int(void)
{
	res = (int)(uc < i);
}

void cmp_uchar_long(void)
{
	res = (int)(uc < l);
}

void cmp_uchar_longlong(void)
{
	res = (int)(uc < ll);
}

void cmp_uint_long(void)
{
	res = (int)(u < l);
}

void cmp_uint_longlong(void)
{
	res = (int)(u < ll);
}

void cmp_ulong_longlong(void)
{
	res = (int)(ul < ll);
}
