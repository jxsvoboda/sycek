/*
 * Comparsion of different enum types.
 */
enum e {
	e1
};

enum f {
	f1
};

void foo(void)
{
	if (e1 < f1)
		return;
	if (e1 <= f1)
		return;
	if (e1 == f1)
		return;
	if (e1 != f1)
		return;
	if (e1 >= f1)
		return;
	if (e1 > f1)
		return;
}
