/*
 * Test flow control constructs
 */

int main(void)
{
	int i;

	/* Do loop */
	i = 0;
	do {
		i++;
	} while (i < 10);

	/* While loop */
	i = 0;
	while (i < 10)
		i++;

	/* While loop with empty body */
	while (true)
		;

	/* For loop */
	for (i = 0; i < 10; i++)
		printf("%d\n", i);

	/* For loop with variable declaration (C99) */
	for (int j = 0; j < 10; j++)
		printf("%d\n", i);

	/* For loop with pointer declaration */
	for (link_t *link = start; link != NULL; link = link->next)
		printf("%p\n", link);

	/* If statement */
	if (i > 0) {
		printf("Positive\n");
	}

	/* If-else statement */
	if (i > 0) {
		printf("Positive\n");
	} else {
		printf("Negative\n");
	}

	/* If-else if-else statement */
	if (i > 0) {
		printf("Positive\n");
	} else if (i < 0) {
		printf("Negative\n");
	} else {
		printf("Zero\n");
	}
}
