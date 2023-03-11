/*
 * Definition of struct in a non-global scope.
 */

void fun(void)
{
	struct foo {
		int a;
	} f;
}
