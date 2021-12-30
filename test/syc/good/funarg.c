/*
 * Function with arguments
 */

int add(int x, int y, int z)
{
	return x + y + z;
}

int funcall_arg(void)
{
	return add(1, 2, 3);
}

int a;

int funcall_varg(void)
{
	a = 1;
	return add(a, a, a);
}
