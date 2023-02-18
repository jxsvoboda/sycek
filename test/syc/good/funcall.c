/*
 * Call function without arguments or return value
 */

int a;

void fun(void)
{
	a = 1;
	return;
}

void call_fun(void)
{
	fun();
}
