/*
 * 16-bit integer multiplication
 *
 * This performs a 16x16 -> 16 bit unsigned multiplication using more
 * primitive operations. This is a great example that needs exactly
 * 7 bytes of temporary storage (16-bit a, b, s, 8-bit cnt), which fits neatly
 * into Z80 registers A, BC, DE, HL.
 *
 * This is a great test for register allocation. It is a direct translation
 * of the assembly algorithm to C. A good optimizing compiler should produce
 * code that is practically identical to the hand-written assembly routine
 * and runs completely in registers.
 */

int mul(int a, int b)
{
	int i, j;
	int s;
	int cnt;

	/* Need to copy arguments since they are not lvalues yet */
	i = a;
	j = b;
	s = 0;

	for (cnt = 0; cnt < 16; cnt++) {
		if (i & 1)
			s += j;
		i >>= 1;
		j <<= 1;
	}

	return s;
}

/* 16-bit integer multiplication using the multiplication operator */
int mulintr(int a, int b)
{
	return a * b;
}

int ga = 10;
int gb = 20;

int gmul(void)
{
	return mul(ga, gb);
}

int gmulintr(void)
{
	return mulintr(ga, gb);
}
