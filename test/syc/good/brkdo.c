/*
 * Break from do loop statement
 */

int a = 1;
int b = 2;

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
