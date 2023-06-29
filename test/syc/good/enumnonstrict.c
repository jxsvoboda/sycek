/*
 * Enum type that has no tag, typedef or instance is not strict.
 * It is regarded just as a collection of integer constants.
 */


enum {
	e1,
	e2,
	e3
};

enum {
	f1,
	f2,
	f3
};

void earith(void)
{
	int i;

	i = e1;
	i = e1 + f1;
	i = e1 + 1;
	i = e1 - f1;
	i = e1 - 1;
	i = e1 * e1;
	i = e1 * f1;
	i = e1 << 1;
	i = e1 >> 1;
}

void ebitwise(void)
{
/*
	Warning: Bitwise operation on signed integers - is this a problem?

	i = e1 & f1;
	i = e1 & 1;
	i = e1 ^ f1;
	i = e1 ^ 1;
	i = e1 | f1;
	i = e1 | 1;
*/
}

void ecompare(void)
{
	if (e1 < 0)
		return;
	if (e1 < f1)
		return;
	if (e1 <= 0)
		return;
	if (e1 <= f1)
		return;
	if (e1 == f1)
		return;
	if (e1 == 0)
		return;
	if (e1 != f1)
		return;
	if (e1 != 0)
		return;
	if (e1 >= f1)
		return;
	if (e1 >= 0)
		return;
	if (e1 > f1)
		return;
	if (e1 > 0)
		return;
}
