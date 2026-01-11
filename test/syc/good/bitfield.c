/*
 * Bit field declaration, reading and writing.
 */

struct s {
	char bf0 : 1;
	char bf1 : 2;
	char : 0;
	int bf3 : 10;
};

char data;

struct s s1;

void bf1_read(void)
{
	data = s1.bf1;
}

void bf1_write(void)
{
	s1.bf1 = data;
}
