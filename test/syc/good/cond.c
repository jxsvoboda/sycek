/*
 * Conditional operator
 */

int c;
int a, b;
long la, lb;
int d;
long ld;

enum e {
	e1,
	e2
};

enum e ea;
enum e eb;
enum e ed;

/* Both operands are the same type */
void cond(void)
{
	d = (c != 0) ? a : b;
}

/* Left operand needs to be extended */
void condxl(void)
{
	ld = (c != 0) ? a : lb;
}

/* Right operand needs to be extended */
void condxr(void)
{
	ld = (c != 0) ? la : b;
}

/* Both operands are truth values */
void cond_truth(void)
{
	if (c != 0 ? a != 0 : b != 0)
		d = 1;
	else
		d = 0;
}

/* Both operands have the same enum type */
void cond_enum(void)
{
	ed = (c != 0) ? ea : eb;
}

struct s {
	int x;
	int y;
};

struct s sa;
struct s sb;
struct s sd;

/* Both operands have the same structure type */
void cond_struct(void)
{
	sd = c != 0 ? sa : sb;
}
