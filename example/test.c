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

int main(void)
{
	return 0;
}
