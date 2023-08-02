/*
 * Struct/union/enum definition inside a cast.
 */

void main(void)
{
	int i;

	i = sizeof(struct foo { int x; } *);
}
