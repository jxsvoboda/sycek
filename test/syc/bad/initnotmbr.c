/*
 * Record type struct s has no member named 'x'.
 */

struct s {
	int a;
	int b;
};

struct s s1 = {
	.x = 0
};
