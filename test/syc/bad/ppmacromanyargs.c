/*
 * '(' expected. (too many arguments to macro)
 */

#define X(a) a

int i = X(1, 2);
