/*
 * Pointer subscripting with pointer as the right operand
 *
 * This should produce 'pointer should be the left operand while indexing'
 * warnings.
 */

int *p;
int idx;
int i;

/* Index[pointer] */
void ptrsubs_left(void)
{
	i = idx[p];
}
