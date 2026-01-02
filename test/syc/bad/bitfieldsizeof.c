/*
 * Sizeof operator applied to bitfield.
 */

struct s {
	char bf1 : 1;
};

struct s s1;

void foo(void)
{
	unsigned u = sizeof(s1.bf1);
}
