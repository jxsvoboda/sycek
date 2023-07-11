/*
 * Conversion from signed to unsigned for non-negative constant
 */

unsigned u;

void foo(void)
{
	/* The constant 1 is signed, but not negative - no warning */
	u = 1;
}
