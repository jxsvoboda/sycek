/*
 * Bit field initialization.
 */

struct s {
	char a : 2;
	char b : 2;
	unsigned char c : 2;
};

struct s s0;

struct s sa = {
	2
};

struct s sab = {
	2,
	-1
};

struct s sabc = {
	2,
	-1,
	3
};

struct s sda = {
	.a = 2
};

struct s sdb = {
	.b = -1
};

struct s sdab = {
	.a = 2,
	.b = -1
};

struct s sdc = {
	.c = 3
};

struct s sdac = {
	.a = 2,
	.c = 3
};

struct s sdbc = {
	.b = -1,
	.c = 3
};

struct s sdabc = {
	.a = 2,
	.b = -1,
	.c = 3
};
