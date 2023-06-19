/*
 * Suspicious logic operation ivolving enums.
 */
enum e {
	e1, e2
};

void foo(void)
{
	int z;
	enum e ee;

	z = e1 && e2;
	z = e1 || 1;
	z = !e1;

	if (e1)
		return;

	while (e1)
		z = 0;

	do {
	} while (e1);

	for (ee = e1; e1; )
		z = 0;

	/* TODO: Ternary operator */
}
