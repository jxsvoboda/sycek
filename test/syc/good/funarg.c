/*
 * Function with arguments
 */

/*
 * Three 16-bit arguments fit into registers
 */

int add3(int a1, int a2, int a3)
{
	return a1 + a2 + a3;
}

int funcall_arg3(void)
{
	return add3(1, 2, 3);
}

int a = 1, b = 2, c = 3;

int funcall_varg3(void)
{
	return add3(a, b, c);
}

/*
 * Additional 16-bit arguments (a4, a5) are passed on the stack
 */

int add5(int a1, int a2, int a3, int a4, int a5)
{
	return a1 + a2 + a3 + a4 + a5;
}

int funcall_arg5(void)
{
	return add5(1, 2, 3, 4, 5);
}
