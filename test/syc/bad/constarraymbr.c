/*
 * Setting readonly variable (const int [][]).
 * Array of constant integers.
 * Const is in declaration specifiers.
 */

const int carr[2][2] = {
	{ 1, 2 },
	{ 3, 4 }
};

void foo(void)
{
	carr[1][1] = 5;
}
