/*
 * Duplicate case value.
 */

int i;

int main(void)
{
	switch (i) {
	case 0:
		break;
	case 1 - 1:
		break;
	}

	return 0;
}
