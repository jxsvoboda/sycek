/*
 * Suspicious arithmetic operation ivolving enums.
 */
enum e {
	e1, e2
};

void foo(void)
{
	int z;
	enum e ee;

	z = e1 * e2;
	z = e1 << 1;
	z = e1 >> 1;

	ee = e1;
	ee *= e1;
	ee <<= e1;
	ee >>= e1;
}
