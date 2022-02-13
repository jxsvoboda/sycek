/*
 * Local variable declaration shadows a local variable
 */

int foo(void)
{
	int a;

	a = 1;
	if (1) {
		int a;
		a = 1;
	}

	return 0;
}
