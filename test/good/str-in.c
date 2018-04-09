/*
 * Character and string tests
 */

/* Character literal */
char e1 = 'a';

/* String literal */
char *e2 = "a";

/* Character literal containing escaped single quote */
char e3 = '\'';

/* String literal containing escaped double quote */
char *e4 = "\"";

/* Character literal containing escaped newline */
char e5 = '\n';

/* String literal containing escaped newline */
char *e6 = "\n";

/* Multipart string literal */
char *e7 = "a" "b" "c";

/* Multipart string literal with macros */
char *e8 = FOO "a" BAR "b" FOOBAR(x, y) "c";

/* Long character literal */
wint_t e9 = L'x';

/* Long string literal */
wchar_t *e10 = L"x";
