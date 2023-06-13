/*
 * Implicit conversion from 'enum f' to different enum type 'enum e'.
*/
enum e {
	e1
} x;

enum f {
	f1
} y;

void main(void)
{
	x = y;
}
