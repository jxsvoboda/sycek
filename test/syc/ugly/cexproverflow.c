/*
 * Integer arithmetic overflow in a constant expression
 */

enum {
	e1 = 30000 + 30000,
	e2 = 0 - 30000 - 30000,
	e3 = -(-32768),
	e4 = 1000 * 1000
};
