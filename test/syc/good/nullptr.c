/*
 * Compare pointer with null pointer.
 */

int *a;

void eq(void)
{
	if (a == (void *)0)
		return;
	if ((void *)0 == a)
		return;
}

void neq(void)
{
	if (a != (void *)0)
		return;
	if ((void *)0 != a)
		return;
}

void lt(void)
{
	if (a < (void *)0)
		return;
	if ((void *)0 < a)
		return;
}

void lteq(void)
{
	if (a <= (void *)0)
		return;
	if ((void *)0 <= a)
		return;
}

void gt(void)
{
	if (a > (void *)0)
		return;
	if ((void *)0 > a)
		return;
}

void gteq(void)
{
	if (a >= (void *)0)
		return;
	if ((void *)0 >= a)
		return;
}
