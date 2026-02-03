/*
 * Return structure by value.
 */

struct s {
	char x;
};

char a;
char b;

struct s foo(void)
{
	struct s s1;

	s1.x = a;
	return s1;
}

void bar(void)
{
	struct s s2;
	s2 = foo();
	b = s2.x;
}
