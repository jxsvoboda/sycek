/*
 * Function with arguments
 */

int add(int x, int y, int z, int w)
{
	return x + y + z;
}

int funcall_arg(void)
{
	return add(1, 2, 3);
}

int a = 1, b = 2, c = 3;

int funcall_varg(void)
{
	return add(a, b, c);
}
