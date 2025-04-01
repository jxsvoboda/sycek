/*
 * Initializer field overwritten (array)
 */

/* The same index specified twice */
int a1[1] = {
	[0] = 1,
	[0] = 2
};

/* Designated field overwritten by non-designated */
int a2[2] = {
	[1] = 1,
	[0] = 0,
	10
};

/* Non-designated field overwritten by designated */
int a3[2] = {
	1,
	2,
	[0] = 3
};

/* Not fully bracketed field overwritten by designated field */
int a4[2][2] = {
	1, 2, 3, 4,
	[0][0] = 5
};
