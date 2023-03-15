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

/* Define anonymous structure with an instance */
struct {
	long l;
} g;

/* Typedef to anonymous structure */
typedef struct {
	int b;
} foo_t;
