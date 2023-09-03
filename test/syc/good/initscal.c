/*
 * Scalar variable initialization
 */

/* Integer */
int a = 42;

/* Pointer to a symbol */
int *p = &a;

/* Pointer that does not point to a symbol */
int *q = (int *)0x8000;

enum e {
	e1,
	e2
};

/* Enum */
enum e f = e2;
