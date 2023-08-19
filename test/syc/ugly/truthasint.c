/*
 * Truth value used as an integer
 */

int a;
int b;
int c;

int foo(void)
{
	c = !(a < b);
	c = a < b;
	c = a <= b;
	c = a == b;
	c = a != b;
	c = a >= b;
	c = (a < b) || (a > b);
	c = (a < b) && (a > b);

	return 0;
}

/* Declare array whose dimension is a truth value */
int d[1 < 0];
