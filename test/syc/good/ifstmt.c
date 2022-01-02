/*
 * If statement
 */

int a, b, c, d;

/* If statement witout else branch */
int if_stmt_1(void)
{
	if (a)
		d = 0;

	return 0;
}

/* If statement with else branch */
int if_stmt_2(void)
{
	if (a)
		d = 0;
	else
		d = 1;

	return 0;
}

/* If statement with else-if and else branch */
int if_stmt_3(void)
{
	if (a)
		d = 0;
	else if (b)
		d = 1;
	else
		d = 2;

	return 0;
}

/* If statement with two else-if branches and else branch */
int if_stmt_4(void)
{
	if (a)
		d = 0;
	else if (b)
		d = 1;
	else if (c)
		d = 2;
	else
		d = 3;

	return 0;
}
