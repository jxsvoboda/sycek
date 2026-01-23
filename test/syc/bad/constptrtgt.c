/*
 * Setting readonly variable (const int *).
 * The pointer points to a const int.
 * Const is in declaration specifiers.
 */

int i = 1;
const int *ci = &i;

void foo(void)
{
	*ci = 2;
}
