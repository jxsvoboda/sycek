/*
 * Passing struct/union by value.
 */
struct s {
	int x;
};

union u {
	int y;
};

struct s s0;
union u u0;

int a;

void getsx(struct s s1)
{
	a = s1.x;
}

void getuy(union u u1)
{
	a = u1.y;
}

void c_getsx(void)
{
	getsx(s0);
}

void c_getuy(void)
{
	getuy(u0);
}
