/*
 * Character and string tests
 */
#include <wchar.h>
#include <uchar.h>

#define FOO "foo"
#define BAR "bar"
#define FOOBAR(a, b) "foo" a "bar" b

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
char *e8 = FOO "a" BAR "b" FOOBAR("a", "b") "c";

/* Wide character literal */
wint_t e9 = L'x';

/* Wide string literal */
wchar_t *e10 = L"x";

/** UTF-8 string literal */
char *e11 = u8"x";

/* UTF-16 character literal */
char16_t e12 = u'x';

/* UTF-16 string literal */
char16_t *e13 = u"x";

/* UTF-32 character literal */
char32_t e14 = U'x';

/* UTF-32 string literal */
char32_t *e15 = U"x";
