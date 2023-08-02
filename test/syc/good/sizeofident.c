/*
 * sizeof(identifier) is ambiguous. It could be sizeof(type-name) where
 * type-name is 'type-identifier' and it could be 'sizeof <expression>' where
 * expression is '(variable-identifer)'.
 *
 * The parser does not have semantic information and will always parse
 * it as an expression. Code generator then needs to handle this case
 * as special and check which variant it is.
 */
typedef long myint_t;

int i = sizeof(myint_t);
int j = sizeof(i);

void set_i(void)
{
	i = sizeof(myint_t);
}

void set_j(void)
{
	j = sizeof(j);
}
