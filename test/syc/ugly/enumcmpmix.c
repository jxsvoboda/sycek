/*
 * Comparsion of enum and non-enum type.
 */
enum e {
	e1
};

void foo(void)
{
	if (e1 < 1)
		return;
	if (e1 <= 1)
		return;
	if (e1 == 1)
		return;
	if (e1 != 1)
		return;
	if (e1 >= 1)
		return;
	if (e1 > 1)
		return;

	if (1 < e1)
		return;
	if (1 <= e1)
		return;
	if (1 == e1)
		return;
	if (1 != e1)
		return;
	if (1 >= e1)
		return;
	if (1 > e1)
		return;
}
