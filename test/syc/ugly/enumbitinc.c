/*
 * Bitwise operation on different enum types.
 */


enum f {
	f1 = 0x1,
	f2 = 0x2,
	f3 = 0x3
};

enum g {
	g1,
	g2
};

void main(void)
{
	enum f x;
	enum g y;
	int z;

	z = x & y;
	z = x ^ y;
	z = x | y;

	x &= y;
	x ^= y;
	x |= y;
}
