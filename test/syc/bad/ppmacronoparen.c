/*
 * When invoking macro with parameters, the opening parenthesis is missing,
 * instead a different character is found.
 */

#define X(a) a

int i = X;
