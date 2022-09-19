/*
 * Computed expression value is not used
 */

int i;

void rvoid(void)
{
}

int rint(void)
{
	return i;
}

/*
 * Statements/expressions that should generate a warning
 */
void warn(void)
{
	/* Function return value not used */
	rint();
	/* Expression value not used */
	1;
	/* Although value of ++i is used, value of 1 + (++i) is not */
	1 + (++i);
	/* Left operand of comma operator is not used */
	1, rvoid();
	/* Right operand is not used */
	++i, 1;

	/*
	 * Values of expressions in first and third statement inside for(...)
	 * are not used.
	 */
	for (1; ; 2) {
	}

	/* Value of i is not used */
	i;

}

/*
 * Statements/expressions that should NOT generate a warning
 */
void nowarn(void)
{
	/* Pre-increment has a side effect */
	++i;
	/* Post-increment has a side effect */
	i++;
	/* Pre-decrement has a side effect */
	--i;
	/* Post-decrement has a side effect */
	i--;

	/* Assignment has a side effect */
	i = 1;
	/* Combined assignment has a side effect */
	i += 1;
	/* Combined assignment has a side effect */
	i -= 1;
	/* Combined assignment has a side effect */
	i *= 1;
	/* Combined assignment has a side effect */
	i &= 1;
	/* Combined assignment has a side effect */
	i |= 1;
	/* Combined assignment has a side effect */
	i ^= 1;

	/* Casting to void silences the warning */
	(void)1;
	/* Casting to void silences the warning */
	(void)1, rvoid();
}
