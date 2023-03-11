/*
 * Fill the screen with black color.
 *
 * To run this example, simply LOAD "" the tape.
 */

int main(void)
{
	unsigned *sp;
	unsigned i;

	sp = (unsigned *)0x4000;

	for (i = 0; i < 0xc00u; ++i)
		sp[i] = 0xffff;
	return 0;
}
