/*
 * Assignment operator with unions
 */

union u1 {
	char c;
};

union u16 {
	long long ll0;
	long long ll1;
};

union u32 {
	union u16 s0;
	union u16 u1;
};

union u64 {
	union u32 s0;
	union u32 u1;
};

union u128 {
	union u64 s0;
	union u64 u1;
};

union u256 {
	union u128 s0;
	union u128 u1;
};

union u257 {
	union u256 s;
	char c;
};

union s512 {
	union u256 s0;
	union u256 u1;
	char c;
};

union u1 f1;
union u1 g1;

void assign_u1(void)
{
	f1 = g1;
}

union u256 f256;
union u256 g256;

void assign_u256(void)
{
	f256 = g256;
}

union u257 f257;
union u257 g257;

void assign_u257(void)
{
	f257 = g257;
}
