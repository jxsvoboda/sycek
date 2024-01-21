/*
 * Mixing truth and integer as conditional operator operands.
 */

int c;
int a;
int b;
int d;

void main(void)
{
	d = (c != 0) ? a : b != 0;
	d = (c != 0) ? a != 0 : b;
}
