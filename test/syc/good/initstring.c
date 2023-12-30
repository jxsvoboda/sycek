/*
 * String literals as initializers.
 */

/* Simple string */
char simple[10] = "Hello!";

/* Concatenated string */
char concat[10] = "H" "e" "l" "l" "o" "!";

/* Unterminated string */
char unterm[6] = "Hello!";

/* String literal containing escaped double quote character */
char bsq[2] = "\'";

/* String literal containing escaped double quote character */
char bsdq[2] = "\"";

/* String literal containing a literal quote character */
char justq[2] = "'";

/* string literal containing escaped question mark character */
char bsqm[2] = "\?";

/* String literal containing escaped backslash character */
char bsbs[2] = "\\";

/* String literal containing 'alarm' escape sequence */
char bsa[2] = "\a";

/* String literal containing 'backspace' escape sequence */
char bsb[2] = "\b";

/* String literal containing 'form feed' escape sequence */
char bsf[2] = "\f";

/* string literal containing 'newline' escape sequence */
char bsn[2] = "\n";

/* String literal containing 'carriage return' escape sequence */
char bsr[2] = "\r";

/* String literal containing 'horizontal tab' escape sequence */
char bst[2] = "\t";

/* String literal containing 'vertical tab' escape sequence */
char bsv[2] = "\v";

/* String literal containing 1-digit octal escape sequence */
char bso1[2] = "\1";

/* String literal containing 2-digit octal escape sequence */
char bso2[2] = "\12";

/* String literal containing 3-digit octal escape sequence */
char bso3[2] = "\123";

/* String literal containing hexadecimal escape sequence */
char bsh[2] = "\x12";

/* Simple wide string */
int wsimple[10] = L"Hello!";

/* Concatenated wide string */
int wconcat[10] = L"H" L"e" L"l" L"l" L"o" L"!";

/* Unterminated wide string */
int wunterm[6] = L"Hello!";

/* Wide string literal containing escaped double quote character */
int wbsq[2] = L"\'";

/* Wide string literal containing escaped double quote character */
int wbsdq[2] = L"\"";

/* Wide string literal containing literal quote character */
int wjustq[2] = L"'";

/* Wide string literal containing escaped question mark character */
int wbsqm[2] = L"\?";

/* Wide string literal containing escaped backslash character */
int wbsbs[2] = L"\\";

/* Wide string literal containing 'alarm' escape sequence */
int wbsa[2] = L"\a";

/* Wide string literal containing 'backspace' escape sequence */
int wbsb[2] = L"\b";

/* Wide string literal containing 'form feed' escape sequence */
int wbsf[2] = L"\f";

/* Wide string literal containing 'newline' escape sequence */
int wbsn[2] = L"\n";

/* Wide string literal containing 'carriage return' escape sequence */
int wbsr[2] = L"\r";

/* Wide string literal containing 'horizontal tab' escape sequence */
int wbst[2] = L"\t";

/* Wide string literal containing 'vertical tab' escape sequence */
int wbsv[2] = L"\v";

/* Wide string literal containing 1-digit octal escape sequence */
int wbso1[2] = L"\7";

/* Wide string literal containing 2-digit octal escape sequence */
int wbso2[2] = L"\77";

/* Wide string literal containing 3-digit octal escape sequence */
int wbso3[2] = L"\777";

/* Wide string literal containing hexadecimal escape sequence */
int wbsh[2] = L"\x1234";
