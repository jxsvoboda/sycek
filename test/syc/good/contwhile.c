/*
 * Continue from while loop statement
 */

int a;
int i;
int j;

void contwhile(void)
{
	i = 0;
	while (i < 10) {
		++i;
		if (i >= a)
			continue;
		j = j + 1;
	}
}
