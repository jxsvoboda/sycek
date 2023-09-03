/*
 * Excess initializer
 *
 * The C standard (C89) specifies that there shall be no more initializers
 * than elements to be initialized. Thus we always treat an excess
 * initializer as an error.
 *
 * Because the compiler handles initialization of arrays within arrays
 * in a different function than outermost arrays, need to test this case
 * as well.
 */

int a[2][2] = { { 1, 2 }, { 1, 2, 3 } };
