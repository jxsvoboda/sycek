/*
 * Lvalue required (acessing member of structure returned from function).
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
	a = foo().x;
}
