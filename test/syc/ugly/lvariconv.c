/*
 * Local variable initialization needs to generate warnings about
 * implicit conversion to the type of the variable.
 * In this case 'truth value used as an integer'.
 */

int foo(void)
{
	int a = 0 < 1;
}
