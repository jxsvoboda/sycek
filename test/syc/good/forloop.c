/*
 * For loop
 */

int i;

int do_stuff(void)
{
	return 0;
}

int for_loop(void)
{
	for (i = 10; i; i = i - 1)
		do_stuff();

	return 0;
}

int for_ever_loop(void)
{
	for (;;) {
	}
}
