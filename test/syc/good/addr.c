/*
 * Address (&)
 */

int a = 2;

int addr(void)
{
	int b;

	b = &a;
	return *b;
}
