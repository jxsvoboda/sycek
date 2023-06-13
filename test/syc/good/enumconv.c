/*
 * Explicit conversion between enum types is okay.
*/
enum e {
	e1
} x;

enum f {
	f1
} y;

void main(void)
{
	x = (enum e)y;
}
