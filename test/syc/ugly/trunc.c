/*
 * Conversion may loose significant digits
 */

char c;
int i;
long l;
long long ll;

void trunc_8_16(void)
{
	c = i;
}

void trunc_8_32(void)
{
	c = l;
}

void trunc_8_64(void)
{
	c = ll;
}

void trunc_16_32(void)
{
	i = l;
}

void trunc_16_64(void)
{
	i = ll;
}

void trunc_32_64(void)
{
	l = ll;
}
