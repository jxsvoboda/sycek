/*
 * Switch statement with bool expression
 */

_Bool b;
char d;

void switchlogic(void)
{
	switch (b) {
	case _False:
		d = 0;
		break;
	case _True:
		d = 1;
		break;
	}
}
