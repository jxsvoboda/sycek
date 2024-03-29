/*
 * Example file for compilation with Syc.
 */

int a = 2, b = 1, c = 0;

int ret_const(void)
{
	return 1;
}

/*
 * Declare the function as a user service routine so that the value
 * can be returned to BASIC
 */
int add_const(void) __attribute__((usr))
{
	/*
	 * Despite being constant, the expression will be translated
	 * into instructions.
	 */
	return 1 + 2 + 3;
}

/** Add two numbers to test passing arguments to function */
int add(int x, int y)
{
	return x + y;
}

int subtract(void)
{
	return a - b;
}

int funcall(void)
{
	return 1 + ret_const();
}

int funcall_arg(void)
{
	return add(1, 2);
}

int funcall_varg(void)
{
	return add(a, b);
}

int read_var(void)
{
	/* Read the contents of initialized global variable */
	return c;
}

int assign_var(void)
{
	/* Variable assignment */
	a = b = 3;

	/* Read variable to verify value has been written properly */
	return a;
}

/* Declare external function */
int putpixel(int x, int y);

/* Call external function */
int callext(void)
{
	putpixel(1, 1);
	putpixel(2, 2);
	putpixel(3, 3);
	putpixel(4, 4);
	return 0;
}

/* While loop */
int while_loop(void)
{
	a = 190;
	while (a) {
		putpixel(a, a);
		a = a - 1;
	}

	return 0;
}

/* Do loop */
int do_loop(void)
{
	a = 190;
	do {
		putpixel(a, a);
		a = a - 1;
	} while (a);

	return 0;
}

/* For loop */
int for_loop(void)
{
	for (a = 190; a; a = a - 1) {
		putpixel(a, a);
	}

	return 0;
}

/* Endless for loop */
int for_ever_loop(void)
{
	for (;;) {
	}
}

/* If statement without else branch */
int if_stmt_1(void)
{
	if (a)
		b = 0;

	return 0;
}

/* If statement with else branch */
int if_stmt_2(void)
{
	if (a)
		b = 0;
	else
		b = 1;

	return 0;
}

/* If statement with else-if and else branch */
int if_stmt_3(void)
{
	if (a)
		c = 0;
	else if (b)
		c = 1;
	else
		c = 2;

	return 0;
}

/* If statement with two else-if branches and else branch */
int if_stmt_4(void)
{
	if (a)
		c = 0;
	else if (b)
		c = 1;
	else if (b + 1)
		c = 2;
	else
		c = 3;

	return 0;
}

/* Logical AND */
int land(void)
{
	return (int)(a && b);
}

/* Logical OR */
int lor(void)
{
	return (int)(a || b);
}

/* Logical NOT */
int lnot(void)
{
	return (int)!a;
}

/* Bitwise AND */
int band(void)
{
	return a & b;
}

/* Bitwise XOR */
int bxor(void)
{
	return a ^ b;
}

/* Bitwise OR */
int bor(void)
{
	return a | b;
}

/** Bitwise NOT */
int bnot(void)
{
	return ~a;
}

/** Shift left */
int shl(void)
{
	return a << b;
}

/** Shift right */
int shr(void)
{
	return a >> b;
}

/* Multiplication */
int mul(void)
{
	return a * b;
}

/* Less than */
int lt(void)
{
	return (int)(a < b);
}

/* Less than or equal */
int lteq(void)
{
	return (int)(a <= b);
}

/* Greater than */
int gt(void)
{
	return (int)(a > b);
}

/* Greater than or equal */
int gteq(void)
{
	return (int)(a >= b);
}

/* Equal */
int eq(void)
{
	return (int)(a == b);
}

/* Not equal */
int neq(void)
{
	return (int)(a != b);
}

/* Add assign */
int add_assign(void)
{
	return a += b;
}

/* Subtract assign */
int sub_assign(void)
{
	return a -= b;
}

/* Mul assign */
int mul_assign(void)
{
	return a *= b;
}

/* Shift left assign */
int shl_assign(void)
{
	return a <<= b;
}

/* Shift right assign */
int shr_assign(void)
{
	return a >>= b;
}

/* Bitwise AND assign */
int band_assign(void)
{
	return a &= b;
}

/* Bitwise XOR assign */
int bxor_assign(void)
{
	return a ^= b;
}

/* Bitwise OR assign */
int bor_assign(void)
{
	return a |= b;
}

/* Preincrement */
int preinc(void)
{
	return ++a;
}

/* Predecrement */
int predec(void)
{
	return --a;
}

/* Postincrement */
int postinc(void)
{
	return a++;
}

/* Postdecrement */
int postdec(void)
{
	return a--;
}

/* Parenthesized expression */
int paren(void)
{
	return (1);
}

/* Comma expression */
int comma(void)
{
	return 1, 2;
}

/* Unary plus */
int uplus(void)
{
	return +a;
}

/* Unary minus */
int uminus(void)
{
	return -a;
}

/* Local variables */
int lvars(void)
{
	int i, j;

	i = 1;
	j = 2;
	i += j;

	return j;
}

/* Parallel local variables of the same name */
int lvarpar(void)
{
	if (1) {
		int i;
		i = 1;
	}

	if (1) {
		int i;
		i = 1;
	}
}

/* Null statement */
int stnull(void)
{
	; /* Note that this produces a NOP */
	return 0;
}

/* 4096 in decimal */
int decval = 4096;
/* 4096 in octal */
int octval = 010000;
/* 4096 in hex */
int hexval = 0x1000;
/* 43981 in hex */
int hexabcd = 0xabcd;
/* 43981 in hex */
int hexABCD = 0xABCD;

/* Fill screen with black pixels (test dereference) */
int fillscr(void)
{
	int i;

	for (i = 0; i < 0x1800; i += 2)
		*(unsigned *)(0x4000 + i) = 0xffff;
	return 0;
}

// XXX Add support for declarators in local variables
int *ptr;

/* Test address operator (&) */
int addr(void)
{
	int i;

	i = 1;
	ptr = &i;
	return *ptr;
}

/* Switch statement */
int stswitch(void)
{
	switch (a) {
	case 0:
		return 0;
	case 1:
		return 10;
	case 2:
		return 20;
	default:
		return 30;
	}
}

/* Break from switch statement */
int brkswitch(void)
{
	int i;

	switch (a) {
	case 1:
		i = 10;
		/* fall through to the next case */
	case 2:
		i = 20;
		break;
	case 3:
		i = 30;
	}

	return i;
}

/* Break from do loop */
int brkdo(void)
{
	int i;

	i = 0;

	do {
		if (a + i == b)
			break;
		++i;
	} while (i < 10);

	return i;
}

/* Break from for loop */
int brkfor(void)
{
	int i;

	for (i = 0; i < 10; i++) {
		if (a + i == b)
			break;
	}

	return i;
}

/* Break from while loop */
int brkwhile(void)
{
	int i;

	i = 0;

	while (i < 10) {
		if (a + i == b)
			break;
		++i;
	}

	return i;
}

/* Continue from do loop */
int contdo(void)
{
	int i;
	int j;

	i = 0;
	do {
		++i;
		if (i == a)
			continue;
		j = j + 1;
	} while (i < 10);

	return i;
}

/* Continue from for loop */
int contfor(void)
{
	int i;
	int j;

	j = 1;
	for (i = 0; i < 10; i++) {
		if (i == a)
			continue;
		j = j + 1;
	}

	return i;
}

/* Continue from while loop */
int contwhile(void)
{
	int i;
	int j;

	i = 0;
	while (i < 10) {
		++i;
		if (i == a)
			continue;
		j = j + 1;
	}

	return i;
}

/* Go to label */
int gotolbl(void)
{
	if (a)
		goto skip;
	b = 1;
skip:
}

int main(void)
{
	return 0;
}
