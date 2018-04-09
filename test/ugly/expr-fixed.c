
/* Initialization '=' at beginning of a line */
int e =
    f + g;

int main(void)
{
	/*
	 * With abstract declarator allowed when parsing idlist in stdecln,
	 * this was misinterpreted as specifier + array declarator and
	 * spaced appropriately
	 */
	a[i] = 0;

	/*
	 * When tried to first parse with expression argument,
	 * this was misinterpreted as sizeof ( (int) (*2) )
	 */
	int a = sizeof(int) * 2;

	/* array[0] must be parsed as an expression, not a type name */
	int b = sizeof(array) / sizeof(array[0]);

	/* Expression */
	int c = sizeof(a * b);

	/* Type name */
	int d = sizeof(foo_t *);

	/* Binary operator at the beginning of a line */
	a = b +
	    c;

	/* Initialization '=' at beginning of a line */
	int e =
	    f + g;
}
