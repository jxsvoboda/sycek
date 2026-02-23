/*
 * Converting to _Bool from truth value.
 */

_Bool b;
char x;
char y;

void from_truth(void)
{
	b = x < y;
}

void pass_bool(_Bool c)
{
	b = c;
}

void pass_from_truth(void)
{
	pass_bool(x < y);
}
