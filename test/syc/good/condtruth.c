/*
 * Conditional operator with _Bool, truth and a mixture of both.
 */
_Bool a, b, c, d;
char x, y;

void foo(void)
{
	d = c ? a : b;
	d = c ? a : (x < y);
	d = c ? (x < y) : b;
	d = c ? (x < y) : (x < y);
	d = (x < y) ? a : b;
	d = (x < y) ? a : (x < y);
	d = (x < y) ? (x < y) : b;
	d = (x < y) ? (x < y) : (x < y);
}
