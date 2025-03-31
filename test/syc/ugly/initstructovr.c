/*
 * Initializer field overwritten (struct)
 */

struct s {
	char a;
	int b;
};

struct t {
	struct s x, y;
};

/* The same member specified twice */
struct s s1 = {
	.a = 1,
	.a = 2
};

/* Designated field overwritten by non-designated */
struct s s2 = {
	.b = 1,
	.a = 0,
	2
};

/* Non-designated field overwritten by designated */
struct s s3 = {
	1,
	2,
	.a = 3
};

/* Not fully bracketed field overwritten by designated field */
/*TODO struct t1 = {
	1, 2, 3, 4,
	.x.a = 5
};*/
