/*
 * Break from while loop statement
 */

int a;
int b;
int i;

void brkwhile(void)
{
	i = 0;

	while (i < 10) {
		if (a + i == b)
			break;
		++i;
	}
}
