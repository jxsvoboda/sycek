/*
 * Nested redefinition of 'struct foo'
 */

struct foo {
	int x;
	int y;
	struct foo {
		int x;
		int y;
	} g;
} f;
