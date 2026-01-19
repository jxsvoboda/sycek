/*
 * Truth value used as an integer.
 * (mixing truth and int in conditonal operator).
 */

int i;
_Bool c;
char x, y;

void foo(void)
{
	i = c ? i : (x < y);
	i = c ? (x < y) : i;
}
