int run_test(void)
{
	int a[16];
	int b[16];
	int i, j;
	int sum;

	j = 0;
	for (i = 0; i < 16; i++)
	{
		a[i] = i + --j;
		b[i] = 16 - i;
	}

	sum = 0;
	for (i = 0; i < 16; i++)
	{
		sum += a[i]++ - b[i];
	}

	return sum;
}
