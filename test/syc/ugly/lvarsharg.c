/*
 * Local variable declaration shadows an argument
 */

int foo(int a)
{
	int a;
	return 0;
}
