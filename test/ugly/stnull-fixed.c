/*
 * Empty statements are allowed only in specific cases, otherwise they
 * should be reported.
 */

int a(void)
{
	/* Empty statement in the body of a function should be reported */
	;
}

DEF(b)
{
	/*
	 * Empty statement in the body of a macro-defined function should
	 * be reported
	 */
	;
}

int c(void)
{
	int i;

	while (1) {
		/* Empty statement in a braced block should be reported. */
		;
	}

	/*
	 * Empty statement as the body of a for loop should be reported.
	 */
	for (i = 0; i < 10; i++)
		;
}
