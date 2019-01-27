/*
 * Declaration style issues
 */

/* Should report mixing function and non-function declarators */
void foo(void), *p;

/* Should report multiple function declarators in a single declaration */
void bar(void), foobar(void);
