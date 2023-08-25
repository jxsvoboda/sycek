/*
 * Converting array to incompatible pointer type
 */

int a[10];
long *p;

/* Convert array to incompatible pointer by means of implicit cast */
void cast_array_ptr(void)
{
	p = a;
}
