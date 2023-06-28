/*
 * Implicit conversion from/to enum.
 */
enum e {
	e1, e2
};

enum e x;
enum e y;

void foo(void)
{
	char c;
	int i;
	long l;

	/* Implicit conversion from 'enum e' to 'int'. */
	i = x;

	/*
	 * Implicit conversion from 'enum e' to 'int'.
	 * Conversion may loose significant digits.
	 */
	c = x;

	/* Implicit conversion from 'int' to 'enum e'. */
	x = i;

	/*
	 * Conversion may loose significant digits.
	 * Implicit conversion fron 'long' to 'enum e'.
	 */
	x = l;
}
