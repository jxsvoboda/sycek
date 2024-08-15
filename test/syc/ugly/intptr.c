/*
 * Integer to pointer conversion
 */

char c;
int i;
long l;
int *ip;

void int_to_ptr_impl(void)
{
	/* Warning: Implicit conversion from integer to pointer. */
	ip = i;
}

void int_to_ptr_expl(void)
{
	/* No warning */
	ip = (int *)i;
}

void char_to_ptr(void)
{
	/* Warning: Converting to pointer from integer of different size. */
	ip = c;
}

void long_to_ptr(void)
{
	/* Warning: Converting to pointer from integer of different size. */
	ip = (int *)l;
}

void fptr(int *p)
{
	(void)p;
}

void callfptr_char(void)
{
	/* Warning: Converting to pointer from integer of different size. */
	fptr(c);
}

void callfptr_long(void)
{
	/* Warning: Converting to pointer from integer of different size. */
	/* Warning: Conversion may loose significant digits. */
	fptr(l);
}
