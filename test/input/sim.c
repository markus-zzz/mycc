int bar(int c)
{
	return c & 0x55;
}

int foo(int a, int b)
{
	return a*2+bar(b)*3;
}


int run_test(void)
{
	int a, b, c;
	a = 5;
	b = 7;
	c = 3;
	return a+b*bar(c)+foo(a,b);
}

