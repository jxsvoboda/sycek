/*
 * Invalid use of 'inline' with local variable declaration.
 */

void foo(void)
{
	inline int b;
}
