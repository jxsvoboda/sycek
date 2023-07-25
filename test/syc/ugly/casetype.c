/*
 * Type checking switch vs. case expressions
 *
 * With strict enums and strict truth type, mixing integer/enum/truth values
 * in switch and case expressions will produce warnings.
 */

enum e {
        e1,
	e2
} x;

enum f {
        f1,
	f2
} y;

int i;

/* Switch based on enum expression */
void swenum(void)
{
	/* Same enum - OK */
	switch (x) {
	case e1:
		break;
	default:
		break;
	}

	/* Incompatible enum */
	switch (x) {
	case f1:
		break;
	default:
		break;
	}

	/* Integer */
	switch (x) {
	case 0:
		break;
	default:
		break;
	}

	/* Truth value */
	switch (x) {
	case 0 < 1:
		break;
	default:
		break;
	}
}

/* Switch based on integer expression */
void swint(void)
{
	/* Enum */
	switch (i) {
	case e1:
		break;
	default:
		break;
	}

	/* Integer - OK */
	switch (i) {
	case 0:
		break;
	default:
		break;
	}

	/* Truth value */
	switch (i) {
	case 0 < 1:
		break;
	default:
		break;
	}
}

/* Switch based on truth value */
void swtruth(void)
{
	/* Enum */
	switch (0 < i) {
	case e1:
		break;
	default:
		break;
	}

	/* Integer */
	switch (0 < i) {
	case 0:
		break;
	default:
		break;
	}

	/* Truth value - OK */
	switch (0 < i) {
	case 0 < 1:
		break;
	default:
		break;
	}
}
