/*
 * Test declarations
 */

/* Type qualifier (const) in a type qualifier list */
char *const c;

/* XXX Function identifier on a separate line. Should fix and re-wrap. */
void
foo(void)
{
}

/* XXX Function identifier on a separate line. Should fix and re-wrap. */
static void
foo(void)
{
}

/*
 * XXX Function identifier on a separate line, inside pointer declarator.
 * Should fix and re-wrap.
 */
static void *
foo(void)
{
}
