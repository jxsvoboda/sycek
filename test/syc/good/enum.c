/*
 * Enum type
 */


enum e {
    e1,
    e2 = 10,
    e3
};

enum e x1;
enum e x2;
enum e x3;

void eset(void)
{
	x1 = e1;
	x2 = e2;
	x3 = e3;
}