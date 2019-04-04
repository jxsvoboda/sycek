/*
 * Example file for compilation with Syc.
 */

int a, b = 1, c = 2;

int ret_const(void)
{
	return 1;
}

int add(void)
{
	/*
	 * Despite being constant, the expression will be translated
	 * into instructions.
	 */
	return 1 + 2 + 3;
}

int subtract(void)
{
	return 5 - 1 - 1;
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

int main(void)
{
	return 0;
}
