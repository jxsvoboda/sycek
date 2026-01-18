/*
 * Converting to _Bool from integer types of different sizes.
 */

_Bool b;
char c;
int i;
long l;
long long ll;

void from_c(void)
{
	b = (_Bool)c;
}

void from_i(void)
{
	b = (_Bool)i;
}

void from_l(void)
{
	b = (_Bool)l;
}

void from_ll(void)
{
	b = (_Bool)ll;
}
