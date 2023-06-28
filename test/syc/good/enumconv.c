/*
 * Explicit conversion between enum types is okay.
 * Explicit conversion from/to enum is okay.
*/
enum e {
	e1
} x;

enum f {
	f1
} y;

void main(void)
{
	int i;

	x = (enum e)y;
	i = (int)x;
	x = (enum e)1;
}
