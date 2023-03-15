/*
 * union
 */

/* Verify that tag names and normal identifiers do not collide */
int foo;
union foo {
	int x;
} f;

/* Define an union without an instance */
union bar {
	char a;
};
