/*
 * Array as a function argument
 */

int a[10];
int s;
int v;
int i;
int *p;

void arraywrite(int x[10])
{
	x[i] = v;
}

void callwrite(void)
{
	arraywrite(a);
}

void arraysize(int x[10])
{
	/*
	 * Array passed as an argument is really passed as a pointer
	 * and sizeof(x) should return 2 (size of pointer).
	 */
	s = sizeof(x);
}

void callsize(void)
{
	arraysize(a);
}

/* Argument is an array of unspecified size */
void arrayunspec(int x[])
{
	(void)x;
}

void ptradd(int x[10])
{
	p = x + i;
	*p = v;
}

void preinc(int x[10])
{
	++x;
	*x = v;
}

void postinc(int x[10])
{
	x++;
	*x = v;
}

void incr(int x[10])
{
	x += 1;
	*x = v;
}

void toptr(int x[10])
{
	p = x;
	*p = v;
}
