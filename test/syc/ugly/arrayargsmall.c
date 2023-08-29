/*
 * Array passed to function is too small
 */

int a[10];
int v;

void foo(int x[20])
{
	x[0] = v;
}

void callfoo(void)
{
	foo(a);
}
