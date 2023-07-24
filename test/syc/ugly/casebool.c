/*
 * Case expression value is not boolean.
 */

int x;
int y;

void isbool(void)
{
	switch (x < y) {
	case 0:
		break;
	case 1:
		break;
	}
}

void notbool(void)
{
	switch (x < y) {
	case -1:
		break;
	case 2:
		break;
	}
}
