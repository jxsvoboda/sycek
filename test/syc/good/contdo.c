/*
 * Continue from do loop statement
 */

int a = 1;
int b = 2;

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
