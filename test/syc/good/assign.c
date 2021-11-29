/*
 * Example file for compilation with Syc.
 */

int a, b = 1, c = 2;

int assign_var(void)
{
	/* Variable assignment */
	a = b = 3;

	/* Read variable to verify value has been written properly */
	return a;
}
