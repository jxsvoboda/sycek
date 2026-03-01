/*
 * Do NOT warn about type being signed when value can proven nonnegative.
 */

unsigned u;
unsigned char uc;
_Bool b;

enum {
	e_1 = 0x1,
	e_2 = 0x2
};

void foo(void)
{
	/* no bitwise operation on signed integer warning */
	u = ~1;

	/* no bitwise operation on negative number warning */
	u = (u & ~0x1);

	/* no converting signed to unsigned warning */
	u = b ? 1 : 0;

	/* no converting signed to unsigned warning */
	u = b ? (0xff | 0x100) : (0x1f | 0xf1);

	/* no bitwise operation on negative number warning */
	u = u & ~e_2;

	/* no signed to unsigned conversion warning */
	u = (u & (0x80^0xff)) | (b ? 0x80 : 0) | 0;

}
