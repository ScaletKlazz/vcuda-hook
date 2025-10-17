#include <iostream>
#include <math.h>

// CUDA kernel to add elements of two arrays
__global__
void add(int n, float* x, float* y) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = blockDim.x * gridDim.x;
    for (int i = index; i < n; i += stride)
        y[i] = x[i] + y[i];
}

void testTraditionalCudaMemory(int N) {
    float* h_x = new float[N];
    float* h_y = new float[N];

    float* d_x;
    float* d_y;
    cudaMalloc(&d_x, N * sizeof(float));
    cudaMalloc(&d_y, N * sizeof(float));

    // init data
    for (int i = 0; i < N; i++) {
        h_x[i] = 1.0f;
        h_y[i] = 2.0f;
    }

    // int blockSize = 256;
    // int numBlocks = (N + blockSize - 1) / blockSize;

    cudaMemcpy(d_x, h_x, N * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(d_y, h_y, N * sizeof(float), cudaMemcpyHostToDevice);

    // Modify the kernel to use shared memory
    for (int i = 0; i < 10; i++) {
        // Define shared memory size (per block)
        // size_t sharedMemSize = blockSize * sizeof(float);

        // Launch kernel with shared memory
        // add<<<numBlocks, blockSize, sharedMemSize>>>(N, d_x, d_y);
        dim3 gridDim(8, 256, 1);
        dim3 blockDim(256, 1, 1);
        size_t sharedMemBytes = 16384;  // 16KB
        add<<<gridDim, blockDim, sharedMemBytes>>>(N, d_x, d_y);

        cudaDeviceSynchronize();
    }
    cudaMemcpy(h_y, d_y, N * sizeof(float), cudaMemcpyDeviceToHost);

    float maxError = 0.0f;
    for (int i = 0; i < N; i++)
        maxError = fmax(maxError, fabs(h_y[i] - 3.0f));
    std::cout << "Max : " << maxError << std::endl;

    cudaFree(d_x);
    cudaFree(d_y);

    delete[] h_x;
    delete[] h_y;
}

int main(void) {
    int N = 1 << 28;

    std::cout << "Test Data Size: " << (N * sizeof(float) / (1024 * 1024)) << " MB" << std::endl;

    testTraditionalCudaMemory(N);

    return 0;
}
