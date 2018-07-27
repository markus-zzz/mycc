void fill_array(int *b)
{
	int i;
	for (i = 0; i < 16; i = i + 1)
		b[i] = i;
}

int run_test(void)
{
	int a[16];
	int j;
	int sum;

	fill_array(a);
	sum = 0;
	for (j = 0; j < 16; j = j + 1)
		sum = sum + a[j];

	return sum;
}	
