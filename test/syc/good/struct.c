/*
 * struct
 */

/* Verify that tag names and normal identifiers do not collide */
int foo;
struct foo {
	int x;
} f;
