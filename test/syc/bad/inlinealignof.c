/*
 * Invalid use of 'inline' inside sizeof().
 */

int s1;

void foo(void)
{
	s1 = sizeof(inline int);
}
