/*
 * Copyright 2026 Jiri Svoboda
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

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
	unsigned i, j;
	int s;
	unsigned char cnt;

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

int gmul(void) __attribute__((usr))
{
	return mul(ga, gb);
}

int gmulintr(void)  __attribute__((usr))
{
	return mulintr(ga, gb);
}

int main(void)
{
	return 0;
}
