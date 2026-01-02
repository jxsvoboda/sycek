/*
 * Warning: Bitfield 'ebf' is narrower than the values of its type.
 */
enum e {
	e1,
	e2,
	e3,
	e4
};

struct s {
	enum e ebf : 1;
};
