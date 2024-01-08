/*
 * Function pointer
 */

int foo(int a, int b)
{
	return a + b;
}

int (*fp)(int, int) = foo;

int i;

void main(void)
{
	i = (*fp)(1, 2);
}
