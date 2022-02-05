/*
 * Local variable declaration shadows a local variable
 */

int foo(void)
{
	int a;

	if (1) {
		int a;
	}

	return 0;
}
