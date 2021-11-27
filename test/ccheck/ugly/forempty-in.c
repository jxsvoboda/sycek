/*
 * For loop with empty next expression should be reported.
 * These should be re-phrased as a while loop.
 */
int main(void)
{
	int i;

	for (i = 0; i < 10; ) {
		printf("%d,", i++);
		printf("%d\n", i++);
	}
}
