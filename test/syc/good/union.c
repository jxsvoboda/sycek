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

/* Define anonymous structure with an instance */
union {
	long l;
} g;

/* Typedef to anonymous union */
typedef struct {
	int b;
} foo_t;
