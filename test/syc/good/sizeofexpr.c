/*
 * sizeof <expression>
 */

int i;
int sl;
int se;
int sv;

void set_sl(void)
{
	/* sizeof(literal) */
	sl = sizeof 1l;
}

void set_se(void)
{
	/* sizeof(more complex expression) */
	se = sizeof(1l + 1l);
}

void set_sv(void)
{
	/* sizeof(variable) */
	sv = sizeof i;
}
