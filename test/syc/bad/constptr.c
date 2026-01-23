/*
 * Setting readonly pointer (int const *).
 * The pointer is constant.
 * Const is in pointer declarator's type qualifier list.
 */

int i = 1;
int j = 2;
int *const cp = &i;

void foo(void)
{
	cp = &j;
}
