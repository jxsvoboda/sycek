/*
 * Excess initializer (union initialization)
 *
 * In case of union only the first element is initialized. The
 * fact that we get an excess initializer warning for the second
 * initializer hints that this rule is probably observed.
 */

union u {
	int i;
	char c;
};

union u a = { 1, 2 };
