/*
 * Cannot take the address of a bitfield.
 */

struct s {
	char bf1 : 1;
};

struct s s1;
char *p = &s1.bf1;
