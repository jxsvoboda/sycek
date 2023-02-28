/*
 * Subscript index is not an integer.
 */

int *p1;
int *p2;
int i;

/* Pointer[pointer] */
void ptr_subs_ptr(void)
{
	i = p1[p2];
}
