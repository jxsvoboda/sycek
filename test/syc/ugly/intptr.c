/*
 * Integer to pointer conversion
 */

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
	ip = (int *)l;
}
