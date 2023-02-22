/*
 * Explicit pointer cast
 */

unsigned char *uc;
unsigned *u;
unsigned long *ul;
unsigned long long *ull;

void ptr_cast_char_uint(void)
{
	uc = (unsigned char *)u;
}

void ptr_cast_uint_ulong(void)
{
	u = (unsigned *)ul;
}

void ptr_cast_ulong_ulonglong(void)
{
	ul = (unsigned long *)ull;
}
