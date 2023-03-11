/*
 * Definition of struct/union inside another struct/union definition.
 */

struct foo {
	struct bar {
		int x;
	} b;
} f;
