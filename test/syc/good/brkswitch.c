/*
 * Break from switch statement
 */

int a;
int i;

void brkswitch(void)
{
	switch (a) {
	case 1:
		i = 10;
		/* fall through to the next case */
	case 2:
		i = 20;
		break;
	case 3:
		i = 30;
	}
}
