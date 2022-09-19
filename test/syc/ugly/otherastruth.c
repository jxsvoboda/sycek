/*
 * 'Type' used as truth value
 */

int a;
int b;

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

	/* In logical not operator */
	(void)!a;

	/* In logical or operator */
	(void)(a || b);

	/* In logical and operator */
	(void)(a && b);

	return 0;
}
