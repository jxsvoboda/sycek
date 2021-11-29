/*
 * Argument declaration shadows a global-scope declaration
 */

int a;

int foo(int a)
{
	return 0;
}
