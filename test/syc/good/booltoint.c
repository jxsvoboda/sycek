/*
 * Converting to _Bool to integer types of different sizes.
 */

_Bool b;
char c;
int i;
long l;
long long ll;

void to_c(void)
{
	c = (char)b;
}

void to_i(void)
{
	i = (int)b;
}

void to_l(void)
{
	l = (long)b;
}

void to_ll(void)
{
	ll = (long long)b;
}
