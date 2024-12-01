/*
 * Pointer difference
 */

int r[10];
int *a;
int *b;
int d;

void diffptr(void)
{
	d = a - b;
}

void diffptrarr(void)
{
	d = a - r;
}

void diffarrptr(void)
{
	d = r - a;
}
