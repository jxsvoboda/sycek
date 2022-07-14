/*
 * Truth value used as an integer
 */

int a;
int b;
int c;

int foo(void)
{
	a = !a;
	c = a < b;
	c = a <= b;
	c = a == b;
	c = a != b;
	c = a >= b;
	c = a || b;
	c = a && b;

	return 0;
}
