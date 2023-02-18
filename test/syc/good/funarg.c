/*
 * Function with arguments
 */

char c1, c2, c3, c4, c5, c6, c7, c8;
char rc1, rc2, rc3, rc4, rc5, rc6, rc7, rc8;

int i1, i2, i3, i4, i5;
int ri1, ri2, ri3, ri4, ri5;

long l1, l2;
long rl1, rl2;

long long ll1;
long long rll1;

/*
 * Three 16-bit arguments fit into registers
 */

void int3(int a1, int a2, int a3)
{
	ri1 = a1;
	ri2 = a2;
	ri3 = a3;
}

void call_int3(void)
{
	int3(i1, i2, i3);
}

/*
 * Additional 16-bit arguments (a4, a5) are passed on the stack
 */

void int5(int a1, int a2, int a3, int a4, int a5)
{
	ri1 = a1;
	ri2 = a2;
	ri3 = a3;
	ri4 = a4;
	ri5 = a5;
}

int call_int5(void)
{
	int5(i1, i2, i3, i4, i5);
}

/*
 * Up to seven 8-bit arguments fit in regsters.
 */
void char7(char a1, char a2, char a3, char a4, char a5, char a6, char a7)
{
	rc1 = a1;
	rc2 = a2;
	rc3 = a3;
	rc4 = a4;
	rc5 = a5;
	rc6 = a6;
	rc7 = a7;
}

void call_char7(void)
{
	char7(c1, c2, c3, c4, c5, c6, c7);
}

/*
 * Eighth character argument is passed on the stack
 */

void char8(char a1, char a2, char a3, char a4, char a5, char a6, char a7,
    char a8)
{
	rc1 = a1;
	rc2 = a2;
	rc3 = a3;
	rc4 = a4;
	rc5 = a5;
	rc6 = a6;
	rc7 = a7;
	rc8 = a8;
}

void call_char8(void)
{
	char8(c1, c2, c3, c4, c5, c6, c7, c8);
}

/*
 * It is possible to fit, for example, one 32-bit, one 16-bit and
 * one 8-bit argument all to registers. The 32-bit argument is passed
 * in two register pairs (HL, DE). The 16-bit argument is passed in one
 * register pair (BC). The 8-bit argument is passed in an 8-bit register (A).
 */
void long_int_char(long a1, int a2, char a3)
{
	rl1 = a1;
	ri1 = a2;
	rc1 = a3;
}

void call_long_int_char(void)
{
	long_int_char(l1, i1, c1);
}

/*
 * Arguments can be passed partially in registers, partialy on the stack.
 * For a function with 32-bit arguments, the first will be passed in
 * DEHL, the lower half of the secon argument in BC and the upper half
 * on the stack.
 */

void long2(long a1, long a2)
{
	rl1 = a1;
	rl2 = a2;
}

void call_long2(void)
{
	long2(l1, l2);
}

/*
 * For a function with a 64-bit argument, the lower 48-bits are passed
 * in BCDEHL, the upper 16 bits on the stack.
 */

void longlong(long long a1)
{
	rll1 = a1;
}

void call_longlong(void)
{
	longlong(ll1);
}
