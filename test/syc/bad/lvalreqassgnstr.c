/*
 * Lvalue required (assigning to structure returned from function).
 */

struct s {
	char x;
};

char a;

struct s foo(void)
{
	struct s s1;

	s1.x = a;
	return s1;
}

void bar(void)
{
	struct s s2;
	foo() = s2;
}
