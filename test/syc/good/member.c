/*
 * Member access
 */

struct s {
	int x;
	int y;
};

struct s f;

void set_f(void)
{
	f.x = 1;
	f.y = 2;
}

union u {
	int x;
	int y;
};

union u g;

void set_g(void)
{
	g.x = 1;
	g.y = 2;
}
