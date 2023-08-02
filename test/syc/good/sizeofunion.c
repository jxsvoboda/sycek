/*
 * sizeof() applied to a union. The compiler needs to compute union
 * sizes correctly.
 */

union u {
	int x;
	int y;
};

int i = sizeof(union u);
