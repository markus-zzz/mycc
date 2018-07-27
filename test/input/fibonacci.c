void non_recursive(int n, int *res)
{
	int first, second, sum;
	first = 0;
	second = 1;

	while (n > 0)
	{
		sum = first + second;
		first = second;
		second = sum;
		res[--n] = sum;
	}
}

int run_test(void)
{
	int r[16];
	int i;

	non_recursive(16, r);
	
	return r[0] + r[4] + r[9] + r[14];
}

