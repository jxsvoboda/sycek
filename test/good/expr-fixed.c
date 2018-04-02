int main(void)
{
	/*
	 * With abstract declarator allowed when parsing idlist in stdecln,
	 * this was misinterpreted as specifier + array declarator and
	 * spaced appropriately
	 */
	a[i] = 0;
}
