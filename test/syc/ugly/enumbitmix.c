/*
 * Bitwise operation on different enum types.
 */


enum f {
	f1 = 0x1,
	f2 = 0x2,
	f3 = 0x3
};

void main(void)
{
	enum f x;
	int z;

	z = x & 1;
	z = x ^ 1;
	z = x | 1;

	x &= 1;
	x ^= 1;
	x |= 1;
}
