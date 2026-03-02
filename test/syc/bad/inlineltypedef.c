/*
 * Invalid use of 'inline' with typedef in local scope.
 */

void foo(void)
{
	typedef inline int i_t(void);
}
