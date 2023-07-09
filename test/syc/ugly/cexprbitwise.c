/*
 * Bitwise operation on negative number(s)
 */

enum {
	e1 = (-1) & 1,
	e2 = 1 & (-1),
	e3 = (-1) | 1,
	e4 = 1 | (-1),
	e5 = (-1) ^ 1,
	e6 = 1 ^ (-1),
	e7 = ~(-1)
};
