/*
 * Too many arguments to function.
 */

int x;
int y;

void foo(int a, int b)
{
	x = a;
	y = b;
}

int main(void)
{
	foo(10);
	return 0;
}
