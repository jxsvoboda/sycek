/*
 * Gratuitous nested block & local variable shadowing
 */

int foo(void)
{
	int a;

	{
		int a;
	}

	return 0;
}
