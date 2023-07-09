/*
 * Bitwise operation on negative number(s) in a normal expression
 */

void main(void)
{
	int z;

	z = (-1) & 1;
	z = 1 & (-1);
	z = (-1) | 1;
	z = 1 | (-1);
	z = (-1) ^ 1;
	z = 1 ^ (-1);
	z = ~(-1);
}
