/*
 * Pointer comparison
 */

int res;

int *p;
int *q;

void lt(void)
{
	res = (int)(p < q);
}

void lteq(void)
{
	res = (int)(p <= q);
}

void gt(void)
{
	res = (int)(p > q);
}

void gteq(void)
{
	res = (int)(p >= q);
}

void eq(void)
{
	res = (int)(p == q);
}

void neq(void)
{
	res = (int)(p != q);
}
