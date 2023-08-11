/*
 * Return enum from function
 */

enum e {
	e1,
	e2
};

enum e x;
enum e r;

/* Function returning enum */
enum e ret_enum(void)
{
	return x;
}

/* Call function returning enum */
void call_enum(void)
{
	r = ret_enum();
}
