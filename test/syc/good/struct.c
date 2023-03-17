/*
 * struct
 */


/* Declare a struct without an instance */
struct foo;

/* Verify that tag names and normal identifiers do not collide */
int foo;

/* Instantiate a struct that will be defined later */
struct foo f;

/* Define a previously declared struct, with an instance */
struct foo {
	int x;
} g;

/* Define a struct without an instance */
struct bar {
	char a;
};

/* Define anonymous structure with an instance */
struct {
	long l;
} h;

/* Typedef to anonymous structure */
typedef struct {
	int b;
} foo_t;

/* Struct defined in global scope can be used in local scope */
void fun(void) {
	struct foo i;
	(void)i;
}
