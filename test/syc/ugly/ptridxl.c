/*
 * Pointer indexing with pointer as the right operand
 *
 * This should produce 'pointer should be the left operand while indexing'
 * warnings.
 */

int *p;
int idx;
int *dp;

/* Index + pointer */
int *ptridx_left(void)
{
	dp = idx + p;
}
