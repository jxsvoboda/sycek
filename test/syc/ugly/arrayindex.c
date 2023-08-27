/*
 * Array index is negative / out of bounds
 */

int a[10];
int *p;
int r;

void negative(void)
{
	/* Array index is negative */
	r = a[-1];
	p = &a[-1];
}

void outofbounds(void)
{
	/* Array index is out of bounds */
	r = a[10];
	/*
	 * Defined behavior in C, but we cannot easily detect this
	 * as a special case, so we just emit a warning anyway.
	 */
	p = &a[10];
}
