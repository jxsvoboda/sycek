/*
 * Warning: Comparison of incompatible pointer types.
 */

int *p;
long *q;

void foo(void)
{
	if (p < q)
		return;
	if (p <= q)
		return;
	if (p > q)
		return;
	if (p >= q)
		return;
	if (p == q)
		return;
	if (p != q)
		return;
}
