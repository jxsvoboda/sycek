/*
 * Integer arithmetic overflow
 */

void main(void)
{
	int z;

	/*
	 * Detect integer arithmetic overflow in a normal (not constant)
	 * expression, whose value is nevertheless known at compile time.
	 */
	z = 30000 + 30000;
	z = 0 - 30000 - 30000;
}
