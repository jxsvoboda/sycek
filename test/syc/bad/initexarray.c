/*
 * Excess initializer
 *
 * The C standard (C89) specifies that there shall be no more initializers
 * than elements to be initialized. Thus we always treat an excess
 * initializer as an error.
 */

int a[2] = { 1, 2, 3 };
