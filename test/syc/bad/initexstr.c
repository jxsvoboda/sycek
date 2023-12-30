/*
 * Excess initializer characters in string.
 *
 * The C standard (C89) specifies that there shall be no more initializers
 * than elements to be initialized. Thus we always treat an excess
 * initializer as an error.
 */

char a[5] = "Hello!";
