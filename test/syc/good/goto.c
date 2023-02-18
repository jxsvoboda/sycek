/*
 * Go to label
 */

int a, b;

void gotolbl(void)
{
	if (a)
		goto skip;
	b = 1;
skip:
}
