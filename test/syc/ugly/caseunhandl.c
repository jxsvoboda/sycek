/*
 * Enumeration value 'e2' not handled in switch.
 */

enum e {
	e1,
	e2
} x;

void nodef(void)
{
	/* Warning: Enumeration value 'e2' not handled in switch. */
	switch (x) {
	case e1:
		break;
	}
}

void withdef(void)
{
	/* No warning, since there is a default label */
	switch (x) {
	case e1:
		break;
	default:
		break;
	}
}