/*
 * Subscripted object is neither pointer nor array.
 */

int i1;
int i2;
int i;

/* integer[integer] */
void int_subs_int(void)
{
	i = i1[i2];
}
