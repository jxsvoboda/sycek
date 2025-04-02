/*
 * Excess initializer (union initialization)
 *
 * In case of union only the first element can be initialized
 */

union u {
	int i;
	char c;
};

union u a = { 1, 2 };
