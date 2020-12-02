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
	/* Comment at the end of structure */
} foo_t;

int a1; /* Single-line comment spacing */
/** Single-line doc comment spacing */
int a2;
int a3; /**< Trailing doc comment spacing */
/**< Misplaced trailing doc comment */
int a4;

enum {
	/* Comment in an enum */
	a,
	b
	/* Comment at the end of an enum */
} bar_t;

/* Indent of comment before function */
int main(int argc, char *argv[])
{
	/* Indent of comment at beginning of function */
	if (argc < 2)
		/* Comment in a non-parenthesized branch */
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
