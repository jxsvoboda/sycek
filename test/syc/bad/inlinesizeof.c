/*
 * Invalid use of 'inline' inside sizeof().
 */

int a1;

void foo(void)
{
	a1 = alignof(inline int);
}
