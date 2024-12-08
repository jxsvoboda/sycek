/*
 * For an array whose dimension is known (and thus it is known whether that
 * dimension is an int lr an enum) we can warn if the subscript is of
 * a different type.
 */

enum e {
	e1,
	e2
};

int a[e2];
int b[10];

void foo(void)
{
	/* Warning: Implicit conversion from 'int' to 'enum e'. */
	a[0] = 1;
	/* No warning */
	a[e1] = 2;

	/* No warnning */
	b[0] = 3;
	/* Warning: Implicit conversion from 'enum e' to 'int'. */
	b[e1] = 4;
}