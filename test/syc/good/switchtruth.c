/*
 * Switch statement with truth expression
 */

char x;
char y;
char d;

void switchlogic(void)
{
	switch (x < y) {
	case _False:
		d = 0;
		break;
	case _True:
		d = 1;
		break;
	}
}
