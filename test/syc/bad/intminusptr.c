/*
 * Invalid subtraction of int and ^int.
 */

int *p;
int i;
int *dp;

/* Integer - pointer */
void int_minus_ptr(void)
{
	dp = i - p;
}
