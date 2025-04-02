/*
 * Initializer field overwritten (union)
 */

union u {
	char a;
	int b;
};

union u u1 = {
	.a = 1,
	.b = 2
};

struct s {
	int x;
	int y;
};

union v {
	struct s a;
	struct s b;
};

union v v1 = {
	.a.x = 1,
	.b.y = 2
};
