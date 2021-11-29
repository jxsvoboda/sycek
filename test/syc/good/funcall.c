/*
 * Call function without arguments
 */

int fun(void)
{
	return 1;
}

int call_fun(void)
{
	return fun() + fun();
}
