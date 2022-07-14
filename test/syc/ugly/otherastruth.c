/*
 * 'Type' used as truth value
 */

int a;

int foo(void)
{
	/* In if and else-if statement */
	if (a)
		return 1;
	else if (a)
		return 2;

	/* In while statement */
	while (a)
		;

	/* In do .. while statement */
	do {
	} while (a);

	/* In for statement */
	for (; a; a++)
		;

	/* TODO: In logical not operator */

	return 0;
}
