#include <stdio.h>
#include <stdlib.h>

int run_test(void);

int main(int argc, char **argv)
{
	int res = run_test();
	printf("RESULT:0x%08x\n", res);
	return 0;
}
