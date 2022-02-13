/*
 * Go to label
 */

int a, b;

int gotolbl(void)
{
	if (a)
		goto skip;
	b = 1;
skip:
}
