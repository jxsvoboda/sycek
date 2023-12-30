/*
 * String literals as initializers.
 */

/* Simple string */
char simple[10] = "Hello!";

/* String literal containing escaped double quote character */
char bsq[10] = "\'";

/* String literal containing escaped double quote character */
char bsdq[10] = "\"";

/* String literal containing a literal quote character */
char justq[10] = "'";

/* string literal containing escaped question mark character */
char bsqm[10] = "\?";

/* String literal containing escaped backslash character */
char bsbs[10] = "\\";

/* String literal containing 'alarm' escape sequence */
char bsa[10] = "\a";

/* String literal containing 'backspace' escape sequence */
char bsb[10] = "\b";

/* String literal containing 'form feed' escape sequence */
char bsf[10] = "\f";

/* string literal containing 'newline' escape sequence */
char bsn[10] = "\n";

/* String literal containing 'carriage return' escape sequence */
char bsr[10] = "\r";

/* String literal containing 'horizontal tab' escape sequence */
char bst[10] = "\t";

/* String literal containing 'vertical tab' escape sequence */
char bsv[10] = "\v";

/* String literal containing 1-digit octal escape sequence */
char bso1[10] = "\1";

/* String literal containing 2-digit octal escape sequence */
char bso2[10] = "\12";

/* String literal containing 3-digit octal escape sequence */
char bso3[10] = "\123";

/* String literal containing hexadecimal escape sequence */
char bsh[10] = "\x12";

/* Simple wide string */
int wsimple[10] = L"Hello!";

/* Wide string literal containing escaped double quote character */
int wbsq[10] = L"\'";

/* Wide string literal containing escaped double quote character */
int wbsdq[10] = L"\"";

/* Wide string literal containing literal quote character */
int wjustq[10] = L"'";

/* Wide string literal containing escaped question mark character */
int wbsqm[10] = L"\?";

/* Wide string literal containing escaped backslash character */
int wbsbs[10] = L"\\";

/* Wide string literal containing 'alarm' escape sequence */
int wbsa[10] = L"\a";

/* Wide string literal containing 'backspace' escape sequence */
int wbsb[10] = L"\b";

/* Wide string literal containing 'form feed' escape sequence */
int wbsf[10] = L"\f";

/* Wide string literal containing 'newline' escape sequence */
int wbsn[10] = L"\n";

/* Wide string literal containing 'carriage return' escape sequence */
int wbsr[10] = L"\r";

/* Wide string literal containing 'horizontal tab' escape sequence */
int wbst[10] = L"\t";

/* Wide string literal containing 'vertical tab' escape sequence */
int wbsv[10] = L"\v";

/* Wide string literal containing 1-digit octal escape sequence */
int wbso1[10] = L"\7";

/* Wide string literal containing 2-digit octal escape sequence */
int wbso2[10] = L"\77";

/* Wide string literal containing 3-digit octal escape sequence */
int wbso3[10] = L"\777";

/* Wide string literal containing hexadecimal escape sequence */
int wbsh[10] = L"\x1234";
