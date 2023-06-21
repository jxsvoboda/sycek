/*
 * Suspicious arithmetic operation involving enums
 */
enum e {
	e1, e2
};

enum e x;
enum e y;

void foo(void)
{
	int z;

	z = x + y;
}
