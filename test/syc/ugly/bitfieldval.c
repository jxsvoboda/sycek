/*
 * Value does not fit in bit field.
 */

struct s {
	char bf0 : 1;
	char bf1 : 2;
	char : 0;
	int bf3 : 10;
};

struct s s1;

void foo(void)
{
	s1.bf1 = -3; /* does not fit */
	s1.bf1 = -2; /* OK */
	s1.bf1 = -1; /* OK */
	s1.bf1 = 0; /* OK */
	s1.bf1 = 1; /* OK */
	s1.bf1 = 2; /* does not fit */
}

struct s s2 = {
	.bf1 = 2 /* does not fit */
};
