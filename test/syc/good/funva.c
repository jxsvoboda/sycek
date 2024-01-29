/*
 * Function with variadic arguments
 */

void fun(int, ...);

int f;
char a;
char b;
char c;

void call_fun(void)
{
	fun(f, a, b, c);
}
