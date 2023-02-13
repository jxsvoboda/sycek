/*
 * Integer zero extension
 */

unsigned char uc;
unsigned u;
unsigned long ul;
unsigned long long ull;

void zrext_16_8(void)
{
	u = uc;
}

void zrext_32_8(void)
{
	ul = uc;
}

void zrext_32_16(void)
{
	ul = u;
}

void zrext_64_8(void)
{
	ull = uc;
}

void zrext_64_16(void)
{
	ull = u;
}

void zrext_64_32(void)
{
	ull = ul;
}
