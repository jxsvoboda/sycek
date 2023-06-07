/*
 * Record type struct s has no member named 'z'.
 */

struct s {
	int x;
	int y;
};

struct s f;
struct s *p;

void set_f(void)
{
	p = &f;

	p->z = 1;
}
