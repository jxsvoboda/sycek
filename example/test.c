/*
 * Example file for compilation with Syc.
 */

int a, b = 1, c = 2;

int ret_const(void)
{
	return 1;
}

int add_const(void)
{
	/*
	 * Despite being constant, the expression will be translated
	 * into instructions.
	 */
	return 1 + 2 + 3;
}

/** Add two numbers to test passing arguments to function */
int add(int x, int y)
{
	return x + y;
}

int subtract(void)
{
	return 5 - 1 - 1;
}

int funcall(void)
{
	return 1 + ret_const();
}

int funcall_arg(void)
{
	return add(1, 2);
}

int funcall_varg(void)
{
	a = 1;
	return add(a, a);
}

int read_var(void)
{
	/* Read the contents of initialized global variable */
	return c;
}

int assign_var(void)
{
	/* Variable assignment */
	a = b = 3;

	/* Read variable to verify value has been written properly */
	return a;
}

/* Declare external function */
int putpixel(int x, int y);

/* Call external function */
int callext(void)
{
	putpixel(1, 1);
	putpixel(2, 2);
	putpixel(3, 3);
	putpixel(4, 4);
	return 0;
}

/* While loop */
int while_loop(void)
{
	a = 190;
	while (a) {
		putpixel(a, a);
		a = a - 1;
	}

	return 0;
}

/* Do loop */
int do_loop(void)
{
	a = 190;
	do {
		putpixel(a, a);
		a = a - 1;
	} while (a);

	return 0;
}

int main(void)
{
	return 0;
}
