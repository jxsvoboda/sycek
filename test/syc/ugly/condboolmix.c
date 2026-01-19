/*
 * Imlicit conversion from '_Bool' to 'int'.
 * (mixing bool and int in conditonal operator).
 */

_Bool b, c;
int i;

void foo(void)
{
	i = c ? i : b;
	i = c ? b : i;
}
