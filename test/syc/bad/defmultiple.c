/*
 * Multiple default labels in switch statement.
 */

int i;

int main(void)
{
	switch (i) {
	default:
	default:
		break;
	}

	return 0;
}
