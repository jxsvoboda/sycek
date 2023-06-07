/*
 * Indirect member access
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

	p->x = 1;
	p->y = 2;
}

union u {
	int x;
	int y;
};

union u g;
union u *q;

void set_g(void)
{
	q = &g;

	q->x = 1;
	q->y = 2;
}
