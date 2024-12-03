/*
 * Unsigned 16-bit integer division using both more primitive operations
 * (div16) and using the division operator.
 */

#include <stdio.h>

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
