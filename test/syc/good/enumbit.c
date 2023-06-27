/*
 * Bitwise operation on enum type.
 */


enum f {
	f1 = 0x1,
	f2 = 0x2,
	f3 = 0x4
};

void main(void)
{
	enum f x;
	enum f y;
	enum f z;

	z = x & y;
	z = x ^ y;
	z = x | y;
	z = ~x;

	x &= y;
	x ^= y;
	x |= y;
}
