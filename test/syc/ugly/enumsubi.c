/*
 * Suspicious arithmetic operation involving enums.
 */
enum e {
	e1, e2
};

enum e x;

void foo(void)
{
	int z;

	z = 1 - x;
}
