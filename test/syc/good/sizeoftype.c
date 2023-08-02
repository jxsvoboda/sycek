/*
 * sizeof(typename)
 */

int sl;
int sp;

void set_sl(void)
{
	/* Only with declaration specifier */
	sl = sizeof(long);
}

void set_sp(void)
{
	/* With declarator */
	sp = sizeof(long *);
}
