/*
 * Do loop
 */

int i;

int do_stuff(void)
{
	return 0;
}

int do_loop(void)
{
	i = 10;

	do {
		do_stuff();
		i = i - 1;
	} while (i);

	return 0;
}
