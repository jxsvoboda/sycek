/*
 * Designated array initialization (C99)
 */

/* Fill in uninitialized fields of basic type */
int i1[10] = {
};

int i1sz = sizeof(i1);

/* Fill in uninitialized fields of pointer type */
void *p1[10] = {
};

int p1sz = sizeof(p1);

struct s {
	int x;
};

/* Fill in uninitialized fields of record type */
struct s s1[10] = {
};

int s1sz = sizeof(s1);

enum e {
	ex,
	ey
};

/* Fill in uninitialized fields of enum type */
enum e e1[10] = {
};

int e1sz = sizeof(e1);

/* Elements listed out of order */
int i2[6] = {
	[4] = 4,
	[3] = 3,
	[2] = 2,
	[1] = 1
};

/* Array size not specified */
int i3[] = {
	[3] = 3
};

int i3sz = sizeof(i3);

/* Mixing designated and non-designated fields */
int i4[6] = {
	[3] = 3,
	[4] = 4,
	5,
	[1] = 1,
	2
};
