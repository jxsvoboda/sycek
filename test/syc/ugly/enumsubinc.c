/*
 * Subtracting incompatible enum types.
 */
enum e {
	e1, e2
};

enum f {
	f1, f2
};

enum e x;
enum f y;

void foo(void)
{
	int z;

	z = x - y;
}
