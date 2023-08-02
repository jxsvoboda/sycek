/*
 * sizeof() applied to a structure. The compiler needs to compute structure
 * sizes correctly.
 */

struct s {
	int x;
	int y;
};

int i = sizeof(struct s);
