/*
 * Local variable declaration shadows a global-scope declaration
 */

int a;

int foo(void)
{
	int a;
	return 0;
}
