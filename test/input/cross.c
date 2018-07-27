int sub(int a, int b)
{
	return a - b;
}

int foo(int a, int b)
{
	return sub(b, a);
}

int run_test(void)
{
	return foo(7,5);
}
