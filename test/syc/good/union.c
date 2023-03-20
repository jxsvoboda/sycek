/*
 * union
 */


/* Declare a union without an instance */
union foo;

/* Verify that tag names and normal identifiers do not collide */
int foo;

/* Instantiate a union that will be defined later */
union foo f;

/* Define a previously declared union, with an instance */
union foo {
	int x;
} g;

/* Define a union without an instance */
union bar {
	char a;
};

/* Define anonymous union with an instance */
union {
	long l;
} h;

/* Typedef to anonymous union */
typedef union {
	int b;
} foo_t;

/* Union defined in global scope can be used in local scope */
void fun(void) {
	union foo i;
	(void)i;
}
