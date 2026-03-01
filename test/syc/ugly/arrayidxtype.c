/*
 * Array designator is of type 'x', but index should be of type 'y'.
 */

/* strict enum type */
enum e {
	e_1 = 0x1,
	e_2 = 0x2,
	e_3 = 0x3
};

/* another strict enum type */
enum f {
	f_3 = 0x3
};

/* limit for enum e */
enum {
	e_limit = e_3 + 1
};

/* array with explicitly specified index type */
int a[e_limit] = {
	/* incorrect type */
	[1] = 0,
	[e_2] = 1
};

/* array with implicitly specified index type */
int b[] = {
	/* index type is defined by the first designator */
	[e_1] = 0,
	/* incorrect type */
	[2] = 1,
	/* incorrect type */
	[f_3] = 2
};

/* normal code */
void foo(void)
{
	/* index type is checked */
	a[e_1] = 0;
	/* index type is checked */
	b[e_1] = 0;
}
