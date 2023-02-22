/*
 * Implicit conversion between incompatible pointer types
 */

unsigned char *uc;
unsigned *u;
unsigned long *ul;
unsigned long long *ull;

void ptr_cast_char_uint(void)
{
	uc = u;
}

void ptr_cast_uint_ulong(void)
{
	u = ul;
}

void ptr_cast_ulong_ulonglong(void)
{
	ul = ull;
}
