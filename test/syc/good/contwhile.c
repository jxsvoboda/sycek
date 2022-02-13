/*
 * Continue from while loop statement
 */

int a = 1;
int b = 2;

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
