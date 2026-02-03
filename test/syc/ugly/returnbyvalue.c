/*
 * Return structure by value.
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
