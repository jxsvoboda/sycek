/*
 * Continue from do loop statement
 */

int a;
int i;
int j;

void contdo(void)
{
	i = 0;
	j = 0;
	do {
		++i;
		if (i >= a)
			continue;
		j = j + 1;
	} while (i < 10);
}
