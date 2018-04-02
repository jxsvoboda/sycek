int main(void)
{
	/* A backslash outside of preprocessor line should be an error. */
	printf("Hello " \
	    "world!\n");
}
