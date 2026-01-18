/*
 * Implicit conversion from '_Bool' to '<type>'.
 */

_Bool b;
char c;
int i;
long l;
long long ll;

void to_c(void)
{
	c = b;
}

void to_i(void)
{
	i = b;
}

void to_l(void)
{
	l = b;
}

void to_ll(void)
{
	ll = b;
}
