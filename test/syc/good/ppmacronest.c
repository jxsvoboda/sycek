/*
 * Nested macro expansion.
 */

#define X Y + Z
#define Y 1
#define Z 2

int i = X;
