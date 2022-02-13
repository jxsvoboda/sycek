/*
 * Gratuitous nested block & local variable shadowing
 */

int foo(void)
{
	int a;

	{
		int a;
		a = 1;
	}

	return a;
}
