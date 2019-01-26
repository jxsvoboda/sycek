/*
 * Storage class issues
 */

/* Improper use of 'extern' with function definition */
extern int foo(void)
{
	return 0;
}

/* Improper use of 'extern' with macro-based function definition */
extern BAR(foo)
{
}
