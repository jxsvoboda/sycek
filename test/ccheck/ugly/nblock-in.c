/*
 * Report gratuitous nested block.
 */

int main(int argc)
{
	switch (argc) {
	case 0:
		{
			return 1;
		}
	}

	int x = 1;
	{
	}
}
