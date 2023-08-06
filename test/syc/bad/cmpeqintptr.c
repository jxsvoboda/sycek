/*
 * Comparison of invalid types (int == ptr).
 */

int i;
int *p;

void foo(void)
{
	if (i == p)
		return;
}
