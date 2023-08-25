/*
 * Converting array to pointer
 */

int a[10];
int *p;

/* Convert array to pointer by means of implicit cast */
void cast_array_ptr(void)
{
	p = a;
}

/*
 * Convert array to pointer using the plus operator.
 * (The compiler treats this as a special case)
 */
void index_array_ptr(void)
{
	p = a + 1;
}
