/*
 * Integer promotion
 */

char a, b;
int c;

/* d should be -256 (operands promoted to int before addition) */
int d = (char)128 + (char)128;

void add(void)
{
	/* operands should be promoted to int before addition */
	c = a + b;
}
