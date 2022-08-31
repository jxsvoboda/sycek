/*
 * Function with arguments
 */

/*
 * Three 16-bit arguments fit into registers
 */

int add3(int a1, int a2, int a3)
{
	return a1 + a2 + a3;
}

int funcall_arg3(void)
{
	return add3(1, 2, 3);
}

int a = 1, b = 2, c = 3;

int funcall_varg3(void)
{
	return add3(a, b, c);
}

/*
 * Additional 16-bit arguments (a4, a5) are passed on the stack
 */

int add5(int a1, int a2, int a3, int a4, int a5)
{
	return a1 + a2 + a3 + a4 + a5;
}

int funcall_arg5(void)
{
	return add5(1, 2, 3, 4, 5);
}

char cres;
char c1, c2, c3, c4, c5, c6, c7, c8;

int res;
long lres;
long long llres;

/*
 * Up to seven 8-bit arguments fit in regsters.
 */
void add_char7(char a1, char a2, char a3, char a4, char a5, char a6,
    char a7)
{
	cres = a1 + a2 + a3 + a4 + a5 + a6 + a7;
}

void funcall_char7(void)
{
	add_char7(c1, c2, c3, c4, c5, c6, c7);
}

/*
 * Eighth character argument is passed on the stack
 */

void add_char8(char a1, char a2, char a3, char a4, char a5, char a6,
    char a7, char a8)
{
	cres = a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8;
}

void funcall_char8(void)
{
	add_char8(c1, c2, c3, c4, c5, c6, c7, c8);
}

/*
 * It is possible to fit, for example, one 32-bit, one 16-bit and
 * one 8-bit argument all to registers. The 32-bit argument is passed
 * in two register pairs (HL, DE). The 16-bit argument is passed in one
 * register pair (BC). The 8-bit argument is passed in an 8-bit register (A).
 */
void add_32b_16b_8b(long a1, int a2, char a3)
{
	lres += a1;
	res += a2;
	cres += a3;
}

void funcall_32b_16b_8b(void)
{
	add_32b_16b_8b(1L, 2, c3);
}

/*
 * Arguments can be passed partially in registers, partialy on the stack.
 * For a function with 32-bit arguments, the first will be passed in
 * DEHL, the lower half of the secon argument in BC and the upper half
 * on the stack.
 */

void add_long2(long a1, long a2)
{
	lres = a1 + a2;
}

void funcall_long2(void)
{
	add_long2(1L, 2L);
}

/*
 * For a function with a 64-bit argument, the lower 48-bits are passed
 * in BCDEHL, the upper 16 bits on the stack.
 */

void fun_longlong(long long arg)
{
	llres = arg;
}

void funcall_longlong(void)
{
	fun_longlong(1LL);
}
