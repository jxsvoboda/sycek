/*
 * Pointer to integer conversion
 */

char c;
int i;
long l;
int *ip;

void ptr_to_int_impl(void)
{
	/* Warning: Implicit conversion from pointer to integer. */
	i = ip;
}

void ptr_to_int_expl(void)
{
	/* No warning */
	i = (int)ip;
}

void ptr_to_char(void)
{
	/* Warning: Converting from pointer to integer of different size. */
	/* Warning: Conversion may loose significant digits. */
	c = (char)ip;
}

void ptr_to_long(void)
{
	/* Warning: Converting from pointer to integer of different size. */
	l = (long)ip;
}
