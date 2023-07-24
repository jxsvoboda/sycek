/*
 * Switch expression does not have integer type
 */

struct {
	int x;
} s;

struct x z;

void foo(void)
{
	switch (z) {
	case 1:
		 break;
	}
}
