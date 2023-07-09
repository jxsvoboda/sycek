/*
 * Integer constant expression - type conversion
 */

enum {
	/* Integer -> integer cast */
	e1 = (int)0x12345678ul
};

int z;

void se1(void)
{
	z = e1;
}
