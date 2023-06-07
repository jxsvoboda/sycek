/*
 * Record type struct s has no member named 'z'.
 */

struct s {
	int x;
	int y;
};

struct s f;

void set_f(void)
{
	f.z = 1;
}
