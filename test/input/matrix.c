typedef int ee_u32;
typedef int ee_s16;
typedef int MATRES;
typedef int MATDAT;

void matrix_mul_matrix(ee_u32 N, MATRES *C, MATDAT *A, MATDAT *B) {
	ee_u32 i,j,k;
	for (i=0; i<N; i++) {
		for (j=0; j<N; j++) {
			C[i*N+j]=0;
			for(k=0;k<N;k++)
			{
				C[i*N+j]+=(MATRES)A[i*N+k] * (MATRES)B[k*N+j];
			}
		}
	}
}

void matrix_add_const(ee_u32 N, MATDAT *A, MATDAT val) {
	ee_u32 i,j;
	for (i=0; i<N; i++) {
		for (j=0; j<N; j++) {
			A[i*N+j] += val;
		}
	}
}

ee_s16 matrix_sum(ee_u32 N, MATRES *C, MATDAT clipval) {
	MATRES tmp=0,prev=0,cur=0;
	ee_s16 ret=0;
	ee_u32 i,j;
	for (i=0; i<N; i++) {
		for (j=0; j<N; j++) {
			cur=C[i*N+j];
			tmp+=cur;
			if (tmp>clipval) {
				ret+=10;
				tmp=0;
			} else {
				ret += (cur>prev) ? 1 : 0;
			}
			prev=cur;
		}
	}
	return ret;
}

int run_test(void)
{
	int a[16];
	int b[16];
	int c[16];
	int s;
	int i, j;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			a[i*4+j] = i*17+j*2;
			b[i*4+j] = i*34+j*6;
		}
	}

	matrix_mul_matrix(4, c, a, b);

	matrix_add_const(4, c, 10);

	s = matrix_sum(4, c, 0x8800);

	return s;
}
