/*
 * Redefinition of 'union foo'
 */

union foo {
	union foo {
		int x;
	} f;
	int y;
} f;
