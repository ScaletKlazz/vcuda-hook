#ifndef UTIL_DEFINDE
#define UTIL_DEFINDE 


#define STRINGIFY(x) #x
#define CUDA_SYMBOL_STRING(x) STRINGIFY(x)

#define NO_HOOK reinterpret_cast<void*>(static_cast<intptr_t>(-1))

#endif