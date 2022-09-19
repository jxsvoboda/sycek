/*
 * While loop
 */

int i;

void do_stuff(void)
{
	return;
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
