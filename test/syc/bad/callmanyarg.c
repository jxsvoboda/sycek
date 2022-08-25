/*
 * Too many arguments to function.
 */

int x;

void foo(int a)
{
	x = a;
}

int main(void)
{
	foo(10, 20);
	return 0;
}
