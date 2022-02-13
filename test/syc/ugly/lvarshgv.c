/*
 * Local variable declaration shadows a global-scope declaration
 */

int a;

int foo(void)
{
	int a;
	a = 1;
	return 0;
}
