typedef char ee_u8;
typedef short ee_u16;
typedef int ee_u32;
typedef short ee_s16;
typedef int MATRES;
typedef int MATDAT;

int printf(const char *str, ...);

/* Function: crc*
	Service functions to calculate 16b CRC code.

*/
ee_u16 crcu8(ee_u8 data, ee_u16 crc )
{
	ee_u8 i=0,x16=0,carry=0;

	for (i = 0; i < 8; i++)
    {
		x16 = (ee_u8)((data & 1) ^ ((ee_u8)crc & 1));
		data >>= 1;

		if (x16 == 1)
		{
		   crc ^= 0x4002;
		   carry = 1;
		}
		else 
			carry = 0;
		crc >>= 1;
		if (carry != 0)
		   crc |= 0x8000;
		else
		   crc &= 0x7fff;
    }
	return crc;
} 
ee_u16 crcu16(ee_u16 newval, ee_u16 crc) {
	crc=crcu8( (ee_u8) (newval)				,crc);
	crc=crcu8( (ee_u8) ((newval)>>8)	,crc);
	return crc;
}
ee_u16 crc16(ee_s16 newval, ee_u16 crc) {
	return crcu16((ee_u16)newval, crc);
}
ee_u16 crcu32(ee_u32 newval, ee_u16 crc) {
	crc=crc16((ee_s16) newval		,crc);
	crc=crc16((ee_s16) (newval>>16)	,crc);
	return crc;
}

void printmatorig(MATDAT *A, ee_u32 N, char *name) {
	ee_u32 i,j;
	printf("Matrix %s [%dx%d]:\n",name,N,N);
	for (i=0; i<N; i++) {
		for (j=0; j<N; j++) {
			if (j!=0)
				printf(",");
			printf("%d",A[i*N+j]);
		}
		printf("\n");
	}
}

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

void printmat(int N, MATDAT *A)
{
	int i, j;
	for (i = 0; i < N; i++)
	{
		for (j = 0; j < N; j++)
		{
			printf("A[%d][%d] = %d ", i, j, A[i*N+j]);
		}
		printf("\n");
	}
}

int run_test(void)
{
	int a[4];
	int b[4];
	int c[4];
	int s;
	int i;
	ee_u16 crc = 0;
	
	
	a[0] = 1;
	a[1] = 2;
	a[2] = 3;
	a[3] = 4;

	b[0] = 11;
	b[1] = 12;
	b[2] = 13;
	b[3] = 14;

	matrix_mul_matrix(2, c, a, b);
	printmatorig(c, 2, "matrix mul matrix");

	matrix_add_const(2, c, 100);
	printmatorig(c, 2, "matrix add constant");

	s = matrix_sum(2, c, 150);
	printf("sum: %d\n", s);

	for (i = 0; i < 4; i++)
	{
		crc = crcu32(c[i], crc);
		printf("crc[%d] = 0x%04hx\n", i, crc);
	}

	return (ee_u32)crc;
}
