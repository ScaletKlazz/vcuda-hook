#include <cuda_runtime.h>
#include <iostream>

static void checkCuda(cudaError_t result, const char *message) {
    if (result != cudaSuccess) {
        std::cerr << message << ": " << cudaGetErrorString(result) << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

int main() {
    constexpr size_t bytes = 256 * 1024 * 1024;  // allocate 256MB
    void *device_ptr = nullptr;

    checkCuda(cudaMalloc(&device_ptr, bytes), "cudaMalloc failed");
    std::cout << "Allocated " << bytes << " bytes on GPU at address " << device_ptr << std::endl;

    checkCuda(cudaFree(device_ptr), "cudaFree failed");
    std::cout << "Freed device memory" << std::endl;

    return 0;
}
