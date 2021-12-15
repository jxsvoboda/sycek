/*
 * Declaration style issues
 */

/* Should report mixing function and non-function declarators */
void foo(void), *p;

/* Should report multiple function declarators in a single declaration */
void bar(void), foobar(void);

/* Function pointers are not functions, so should not report anything */
void (*p1)(void), (*p2)(void);

/* Mixing pointer to function with something else should be okay, too */
void (*p3)(void), *q;
