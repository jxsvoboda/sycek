/*
 * Continue from for loop statement
 */

int a = 1;
int b = 2;

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
