/*
 * Non-static '...' was previously declared as static.
 *
 * Note that function declaration, function definitions and variable
 * declaration are handled by separate code paths and need to be tested
 * individually.
 */

static int x;
static int foo(void);
static int bar(void);

int foo(void)
{
}

int bar(void);

int x;
