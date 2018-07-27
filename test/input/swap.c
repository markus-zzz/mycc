int run_test(void)
{
	int a;
	int b;
	int i;

	a = 5;
	b = 7;

	for (i = 0; i < 5; i = i + 1)
	{
		int t;
		t = b;
		b = a;
		a = t;
	}

	return a - b;
}
