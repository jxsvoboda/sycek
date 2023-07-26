/*
 * Case expression value is not in enum.
 */

enum e {
	e1,
	e2
} x;

void inenum(void)
{
	switch (x) {
	case e1:
		break;
	case e2:
		break;
	}
}

void notinenum(void)
{
	switch (x) {
	case e2 + 1:
		break;
	default:
		break;
	}
}
