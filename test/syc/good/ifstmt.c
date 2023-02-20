/*
 * If statement
 */

int a, b, c, d;

/* If statement without else branch */
void if_stmt_1(void)
{
	if (a)
		d = 0;
}

/* If statement with else branch */
void if_stmt_2(void)
{
	if (a)
		d = 0;
	else
		d = 1;
}

/* If statement with else-if and else branch */
void if_stmt_3(void)
{
	if (a)
		d = 0;
	else if (b)
		d = 1;
	else
		d = 2;
}

/* If statement with two else-if branches and else branch */
void if_stmt_4(void)
{
	if (a)
		d = 0;
	else if (b)
		d = 1;
	else if (c)
		d = 2;
	else
		d = 3;
}
