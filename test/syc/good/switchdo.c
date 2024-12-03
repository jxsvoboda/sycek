/*
 * Switch statement containing do loop
 */

int a;
int b;

void stswitch(void)
{
	switch (a) {
	case 1:
		do {
			if (b == 10)
				break;
		} while (--b > 0);
		break;
	default:
		break;
	}
}
