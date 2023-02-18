/*
 * Continue from for loop statement
 */

int a;
int i;
int j;

void contfor(void)
{
	j = 0;
	for (i = 0; i < 10; i++) {
		if (i >= a)
			continue;
		j = j + 1;
	}
}
