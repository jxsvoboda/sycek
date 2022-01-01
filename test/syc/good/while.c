/*
 * While loop
 */

int i;

int do_stuff(void)
{
	return 0;
}

int while_loop(void)
{
	i = 10;

	while (i) {
		do_stuff();
		i = i - 1;
	}

	return 0;
}
