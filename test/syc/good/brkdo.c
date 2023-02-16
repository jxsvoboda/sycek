/*
 * Break from do loop statement
 */

int a;
int b;
int i;

void brkdo(void)
{
	i = 0;
	do {
		if (a + i == b)
			break;
		++i;
	} while (i < 10);
}
