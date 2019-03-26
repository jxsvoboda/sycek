/*
 * Example file for compilation with Syc.
 */

int a, b = 1, c = 2;

int foo(void)
{
	/*
	 * Despite being constant, the expression will be translated
	 * into instructions.
	 */
	return 1 + 2 + 3;
}

int bar(void)
{
	/* Read the contents of initialized global variable */
	return c;
}

int assign_var(void)
{
	/* Variable assignment */
	a = 3;

	/* Read variable to verify value has been written properly */
	return a;
}

int main(void)
{
	return 0;
}
