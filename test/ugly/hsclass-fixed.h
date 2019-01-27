/*
 * Header file storage class issues
 */

/* Non-static function defined in a header */
int foo(void)
{
	return 0;
}

/* Non-static macro-based definition of function in a header */
BAR(foo)
{
}
