/*
 * Break from for loop statement
 */

int a = 1;
int b = 2;

int brkfor(void)
{
	int i;

	for (i = 0; i < 10; i++) {
		if (a + i == b)
			break;
	}

	return i;
}
