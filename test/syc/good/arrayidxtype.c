/*
 * Array index type checking (strict enum).
 */

/* strict enum type */
enum e {
	e_1 = 0x1,
	e_2 = 0x2
};

/* limit for enum e */
enum {
	e_limit = e_2 + 1
};

/* array with explicitly specified index type */
int a[e_limit] = {
	/* designator index type is checked */
	[e_1] = 0,
	[e_2] = 1
};

/* array with implicitly specified index type */
int b[] = {
	/* index type is defined by the first designator */
	[e_1] = 0,
	/* now the index type is checked */
	[e_2] = 1
};

/* normal code */
void foo(void)
{
	/* index type is checked */
	a[e_1] = 0;
	/* index type is checked */
	b[e_1] = 0;
}
