/*
 * struct
 */

/* Verify that tag names and normal identifiers do not collide */
int foo;
struct foo {
	int x;
} f;

/* Define a struct without an instance */
struct bar {
	char a;
};
