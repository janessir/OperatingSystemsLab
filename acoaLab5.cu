//Q2.1
// Hello from CPU!
// Hello from GPU1[0]!
// Hello from GPU1[1]!
// Hello from GPU1[2]!
// Hello from GPU1[3]!
// Hello from GPU2[0]!
// Hello from GPU2[1]!
// Hello from GPU2[2]!
// Hello from GPU2[3]!
// Hello from GPU2[4]!
// Hello from GPU2[5]!

int main(){
	printf("Hello from CPU!\n");
	hello_GPU1<<<1,4>>>();
	cudaDeviceSynchronize();
	hello_GPU2<<<2,3>>>();
	cudaDeviceSynchronize(); //so that CPU waits for the threads to ex finish before ex return 0;
	return 0;
}

//kernel code
__global__ 
void hello_GPU1(){
	int i = threadIdx.x;
	printf("Hello from GPU1[%d]\n",i);
}
__global__ 
void hello_GPU1(){
	int i = blockIdx.x * blockDim.x + threadIdx.x;
	printf("Hello from GPU2[%d]\n",i);
}

//----------------------------------------

// Q2.2 - Vector Addition
// A 22 13 16 5 
// B 5 22 17 37 
// C 27 35 33 42

//to create 4 threads

//kernel code
__global__
void vectorAdd(int *d_c, int *d_a, int *d_b, int n){
	int i = threadIdx.x;
	d_c[i] = d_a[i] + d_b[i];
}

int main(){
	int n = 4; //size of vector
	//instantiate vectors
	int a[n] = {22, 13, 16, 5};
	int b[n] = {5, 22, 17, 37};
	int c[n];

	//declare pointers to arrary on device mem
	int *d_a, d_b, d_c;

	//assign space for the arrays in GPU global mem
	cudaMalloc((void**)&d_a, sizeof(int) * n);
	cudaMalloc((void**)&d_b, sizeof(int) * n);
	cudaMalloc((void**)&d_c, sizeof(int) * n);

	//copy the array to gpu mem
	cudaMemcpy(d_a, a, sizeof(int)*n, cudaMemcpyHostToDevice);
	cudaMemcpy(d_b, b, sizeof(int)*n, cudaMemcpyHostToDevice);

	//kernel call
	vectorAdd<<<1,4>>>(d_c, d_a, d_b, n);

	//copy result in gpu mem to tt in mm
	cudaMemcpy(c, d_c, sizeof(int)*n, cudaMemcpyDeviceToHost);

	//free up the mem space in gpu
	cudaFree(d_a);
	cudaFree(d_b);
	cudaFree(d_c);

	printf("Result: \n");
	for(int j=0; j<n; j++) 
		printf("c[%d]: %d, ", j, c[j]);

	return 0;
}


//Q2.3 - Dot product
// A 22 13 16 5 
// B 5 22 17 37 
// Answer = 853

//kernel code
__global__
void dotProduct(int *d_c, int *d_a, int *d_b){
	int n = blockDim.x;
	int i = threadIdx.x;
	__shared__ int temp[n];

	temp[i] = d_a[i] * d_b[i];

	if(i == 0){
		int sum = 0;
		for(int j=0; j<n; j++) 
			sum+= temp[j];
		//assign the results to c
		*d_c = sum;
	}
}

int main(){
	int n = 4; //size of vector
	//instantiate vectors
	int a[n] = {22, 13, 16, 5};
	int b[n] = {5, 22, 17, 37};
	int c;

	//declare pointers to arrary on device mem
	int *d_a, d_b, d_c;

	//assign space for the arrays in GPU global mem
	cudaMalloc((void**)&d_a, sizeof(int) * n);
	cudaMalloc((void**)&d_b, sizeof(int) * n);
	cudaMalloc((void**)&d_c, sizeof(int) * n); //????

	//copy the array to gpu mem
	cudaMemcpy(d_a, a, sizeof(int)*n, cudaMemcpyHostToDevice);
	cudaMemcpy(d_b, b, sizeof(int)*n, cudaMemcpyHostToDevice);

	//kernel call
	vectorAdd<<<1,4>>>(d_c, d_a, d_b, n);

	//copy result in gpu mem to tt in mm
	cudaMemcpy(c, d_c, sizeof(int)*n, cudaMemcpyDeviceToHost);

	//free up the mem space in gpu
	cudaFree(d_a);
	cudaFree(d_b);
	cudaFree(d_c);

	printf("Result: %d\n", c);
	return 0;
}























