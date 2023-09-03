/*
 * Array initialization
 */

/* All elements initialized */
int a[4] = { 1, 2, 3, 4 };

/* Not enough initializers for all elements */
int b[4] = { 1, 2 };

/* Array of arrays with all elements initialized */
int c[2][2] = { { 1, 2 }, { 3, 4 } };

/* Array of arrays with some initializers omitted */
int d[2][2] = { { 1 }, { 3 } };
