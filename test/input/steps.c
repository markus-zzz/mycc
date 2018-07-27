int run_test(void)
{
	int i;
	int sum = 0;
	for (i = 0; i < 32; i++)
	{
		sum += i;
		if (sum > 16)
		{
			i++;
		}	
	}

	return sum;
}
