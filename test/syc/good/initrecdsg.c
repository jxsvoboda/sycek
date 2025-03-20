/*
 * Designated record type initialization (C99)
 */

int i;

struct s {
	int x;
};

enum e {
	ex,
	ey
};

/* Structure containing fields of different types */
struct r {
	int ie;
	void *pe;
	struct s se;
	enum e ee;
};

/* Fill in uninitialized structure fields of different types */
struct r r1 = {
};

/* Structure elements listed out of order */
struct r r2 = {
	.ee = ex,
	.se = {
		2
	},
	.pe = &i,
	.ie = 1
};

/* Mixing designated and non-designated structure fields */
struct r r3 = {
	.pe = &i,
	{
		2
	},
	ex
};
