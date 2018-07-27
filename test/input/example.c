int foo(int a, int b)
{
	if (a < 5)
	{
		return a + b;
	}
	else
	{
		return a - b;
	}
}

int run_test(void)
{
	int i;
	int sum;
	int array[16];

	sum = 0;
	for (i = 0; i < 16; i = i + 1)
	{
		array[i] = i;
	}

	for (i = 0; i < 15; i = i + 1)
	{
		sum += foo(array[i], array[i+1]);
	}

	return sum;
}
