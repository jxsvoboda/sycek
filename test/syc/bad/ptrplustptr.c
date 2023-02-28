/*
 * Cannot add ^int and ^int.
 */

int *p1;
int *p2;
int *dp;

/* Pointer + pointer */
void ptr_plus_ptr(void)
{
	dp = p1 + p2;
}
