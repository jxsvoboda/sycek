/*
 * Array as a function argument
 */

int a[10];
int s;
int v;
int i;

void arraywrite(int x[10])
{
	x[i] = v;
}

void arraysize(int x[10])
{
	/*
	 * Array passed as an argument is really passed as a pointer
	 * and sizeof(x) should return 2 (size of pointer).
	 */
	s = sizeof(x);
}

void callwrite(void)
{
	arraywrite(a);
}

void callsize(void)
{
	arraysize(a);
}
