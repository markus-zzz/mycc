int run_test(void)
{
	int a;
	int b;
	int c;
	int i;
	int j;
	int t;

	a = 5;
	b = 7;
	c = a*b;

	for (j = 0; j < 11; j = j + 1) 
	for (i = 0; i < 19; i = i + 1)
	{
		t = b;
		b = a;
		a = t + c;
		if (a < 123) a = a + 15;
	}

	return a - b + c;
}
