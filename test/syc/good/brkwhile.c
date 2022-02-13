/*
 * Break from while loop statement
 */

int a = 1;
int b = 2;

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
