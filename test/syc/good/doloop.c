/*
 * Do loop
 */

int i;

void do_stuff(void)
{
	return;
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
