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
 * Unsigned 16-bit integer division using both more primitive operations
 * (div16) and using the division operator.
 */

static void div_by_zero(void)
{
	while (1 != 0);
}

/*
 * 16-bit unsigned integer division/remainder
 */
void div16(unsigned a, unsigned b, unsigned *rq, unsigned *rr)
{
	unsigned d;
	unsigned q, r;
	unsigned char cnt;

	if (b == 0)
		div_by_zero();

	d = a;
	q = 0;
	r = 0;

	for (cnt = 0; cnt < 16; cnt++) {
		q <<= 1;
		r <<= 1;
		r = r | ((d & 0x8000u) != 0 ? 1 : 0);
		if (r >= b) {
			r -= b;
			q = q | 0x0001;
		}
		d <<= 1;
	}

	*rq = q;
	*rr = r;
}

/* 16-bit unsigned integer division using the division operator */
void divintr16(unsigned a, unsigned b, unsigned *rq, unsigned *rr)
{
	*rq = a / b;
	*rr = a % b;
}

unsigned ga = 13;
unsigned gb = 4;
unsigned gq;
unsigned gr;

void cdiv16(void)
{
	div16(ga, gb, &gq, &gr);
}

void cdivintr16(void)
{
	divintr16(ga, gb, &gq, &gr);
}

int main(void)
{
	return 0;
}
