/*
 * Warnings about constant shifts in a normal expression
 */

void main(void)
{
	int z;

	/* Shift amount exceeds operand width */
	z = 1 << 16;

	/* Shift is negative */
	z = 1 << -1;
}
