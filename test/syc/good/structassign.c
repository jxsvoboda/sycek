/*
 * Assignment operator with structures
 */

struct s1 {
	char c;
};

struct s16 {
	long long ll0;
	long long ll1;
};

struct s32 {
	struct s16 s0;
	struct s16 s1;
};

struct s64 {
	struct s32 s0;
	struct s32 s1;
};

struct s128 {
	struct s64 s0;
	struct s64 s1;
};

struct s256 {
	struct s128 s0;
	struct s128 s1;
};

struct s257 {
	struct s256 s;
	char c;
};

struct s512 {
	struct s256 s0;
	struct s256 s1;
	char c;
};

struct s1 f1;
struct s1 g1;

void assign_s1(void)
{
	f1 = g1;
}

struct s256 f256;
struct s256 g256;

void assign_s256(void)
{
	f256 = g256;
}

struct s257 f257;
struct s257 g257;

void assign_s257(void)
{
	f257 = g257;
}
