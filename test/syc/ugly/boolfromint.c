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
	b = c;
}

void from_i(void)
{
	b = i;
}

void from_l(void)
{
	b = l;
}

void from_ll(void)
{
	b = ll;
}
