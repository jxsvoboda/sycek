/*
 * Preprocessor line containing an unterminated comment, not followed by
 * a newline at the end of file.
 * Make sure we properly terminate the loop in lexer_preproc()
 */
#define FOO /*
