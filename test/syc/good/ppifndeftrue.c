/*
 * #ifndef condition that evaluates to true includes its contents.
 */

#undef FOO
#ifndef FOO
int x;
#endif
