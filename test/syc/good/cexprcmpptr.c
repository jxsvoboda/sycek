/*
 * Constant pointer comparison in constant expression
 *
 * While comparing pointers to symbols (e.g. &i) in a constant expression
 * is not allowed, we can compare pointers that are actually constant
 * (e.g. (void *)0).
 */

int lt0 = (int)((void *)0 < (void *)0);
int lt1 = (int)((void *)0 < (void *)1);
int lteq0 = (int)((void *)1 <= (void *)0);
int lteq1 = (int)((void *)1 <= (void *)1);
int gt0 = (int)((void *)0 > (void *)0);
int gt1 = (int)((void *)1 > (void *)0);
int gteq0 = (int)((void *)0 >= (void *)1);
int gteq1 = (int)((void *)0 >= (void *)0);
int eq0 = (int)((void *)0 == (void *)1);
int eq1 = (int)((void *)0 == (void *)0);
int neq0 = (int)((void *)0 != (void *)0);
int neq1 = (int)((void *)0 != (void *)1);
