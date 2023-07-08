/*
 * Warnings regarding constant shifts
 */

enum {
	/* Shift amount is exceeds operand width */
	e1 = 1 << 16,
	/* Shift is negative */
	e2 = 1 << -1
};
