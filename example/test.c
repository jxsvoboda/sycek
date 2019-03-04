/*
 * Example file for compilation with Syc.
 */

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
