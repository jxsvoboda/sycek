/*
 * Constant expression computing address of indirect struct member
 */

struct s {
	int a;
	int b;
};

struct s c;
int *p = &(&c)->a;
int *q = &(&c)->b;
