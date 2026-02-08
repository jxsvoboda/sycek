/*
 * Definition of struct/union inside another struct/union definition.
 */

struct foo {
	/* Should not define bar tag inside another struct. */
	struct bar {
		int x;
	} b;
} f;

struct foo2 {
	/* Anonymous struct is okay, though. */
	struct {
		int x;
	} b;
} f2;
