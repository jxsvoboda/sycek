/*
 * Comment tests
 *
 * Here we test indentation of comments in various positions
 */

typedef struct foo {
// Double-slash comment inside structure
	int x;
} foo_t;

	// Indent of double-slash comment before function
int main(int argc, char *argv[])
{
// Indent of double-slash comment at beginning of function
	if (argc < 2)
		printf("Argument expected!\n");
// Indent of double-slash comment just after indented statement
}
