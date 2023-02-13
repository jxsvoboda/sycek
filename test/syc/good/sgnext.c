/*
 * Integer sign extension
 */

char c;
int i;
long l;
long long ll;

void sgnext_16_8(void)
{
	i = c;
}

void sgnext_32_8(void)
{
	l = c;
}

void sgnext_32_16(void)
{
	l = i;
}

void sgnext_64_8(void)
{
	ll = c;
}

void sgnext_64_16(void)
{
	ll = i;
}

void sgnext_64_32(void)
{
	ll = l;
}
