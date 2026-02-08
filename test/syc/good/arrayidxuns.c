/*
 * Indexing an array using and unsigned integer. Moreover, the dimension
 * of the array is a constant (i.e. an enum that is not strict).
 */

unsigned pos;

enum {
	my_const = 10
};

/*
 * Even though the dimension is an enum, it is not named so it is considered
 * an integer constant.
 */
char foo[my_const];

char *index_array(void)
{
	/*
	 * Even though the dimension is 'int', no warning regarding
	 * indexing with 'unsigned'.
	 */
	return foo + pos;
}
