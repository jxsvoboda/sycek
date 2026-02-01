/*
 * Passing structure by value.
 */

struct s {
	char x;
};

char origv;
char newv;
char readv;
char finalv;

void foo(struct s s1)
{
	/* Copy out value. */
	readv = s1.x;

	/* This must not affect caller's value. */
	s1.x = newv;
}

void bar(void)
{
	struct s s2;

	/* Starting value */
	s2.x = origv;
	/* foo() should overwrite its copy with this value */
	foo(s2);

	/* This should not cha */
	finalv = s2.x;
}
