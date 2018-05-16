/*
 * Comment tests
 *
 * Here we test indentation of comments in various positions
 */

/** Documentation comment
 *
 * This is a doc comment
 */
typedef struct foo {
	/* Comment inside structure */
	int x;
} foo_t;

/* Indent of comment before function */
int main(int argc, char *argv[])
{
	/* Indent of comment at beginning of function */
	if (argc < 2)
		printf("Argument expected!\n");
	/* Indent of double-slash comment just after indented statement */

	asm volatile (
	    /* Indent of comment inside statement */
	    "nop\n"
	);

	/*
	 * The first line of a block comment
	 * should not contain any text,
	 * nor should the last line.
	 */
}

/* Indent of comment after last declaration */
