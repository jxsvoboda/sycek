/*
 * Breakable space before
 */
void brkspace_before(void)
{
	/* Missing space before declarator */
	int *a = (int*)0;
	/* Two space before declarator */
	int *b = (int  *)0;
	/* Tab instead of space before declarator */
	int *c = (int	*)0;
}

/*
 * Non-breakable space before
 */
void nbspace_before(void)
{
	int i;

	/* Missing space before binary operator */
	i= 2;

	/* Multiple spaces before binary operator */
	i  = 2;

	/* Tab instead of space before binary operator */
	i	= 2;
}

/*
 * Breakable space after
 */
void brkspace_after(void)
{
	/* Missing space after binary operator */
	i =3;

	/* Multiple spaces after binary operator */
	i =  3;

	/* Tab instead of space after binary operator */
	i =	3;
}
