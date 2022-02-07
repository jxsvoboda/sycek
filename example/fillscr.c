/*
 * Fill the screen with black color.
 *
 * To run this example, simply LOAD "" the tape.
 */

int main(void)
{
	int i;

	for (i = 0; i < 0x1800; i += 2)
		*(0x4000 + i) = 0xffff;
	return 0;
}
