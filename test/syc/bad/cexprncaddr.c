/*
 * Expression is not constant.
 *
 * Referencing a variable is allowed in constant expressions, but not
 * in integer constant expressions.
 */

int x;

enum {
	e1 = &x
};
