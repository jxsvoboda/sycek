/*
 * When invoking macro with parameters, the opening parenthesis is missing,
 * instead EOF is found.
 */

#define X(a) a

int i = X