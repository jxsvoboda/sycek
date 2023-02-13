/*
 * Integer truncation
 */

char c;
int i;
long l;
long long ll;

void trunc_8_16(void)
{
	c = (char)i;
}

void trunc_8_32(void)
{
	c = (char)l;
}

void trunc_8_64(void)
{
	c = (char)ll;
}

void trunc_16_32(void)
{
	i = (int)l;
}

void trunc_16_64(void)
{
	i = (int)ll;
}

void trunc_32_64(void)
{
	l = (long)ll;
}
