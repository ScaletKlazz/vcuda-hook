#ifndef UTIL_DEFINDE
#define UTIL_DEFINDE 

#define MACRO_TAG_YES 1
#define MACRO_TAG_NO  0

#define DEVICE_MAX_NUM 8
#define DEVICE_INDEX_CURRENT -1

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define SYMBOL_STRING(x) STRINGIFY_IMPL(x)
#define STRINGIFY_IMPL(x) #x

#define EVAL(x) EVAL_IMPL(x)
#define EVAL_IMPL(x) x

#define CAT(a, b) CAT_IMPL(a, b)
#define CAT_IMPL(a, b) a##b

#define ORI_FUNC(name, return_type, ...) \
    using CAT(EVAL(name), _func_ptr) = return_type (*)(__VA_ARGS__); \
    CAT(EVAL(name), _func_ptr) CAT(ori_, EVAL(name)) = nullptr;

#endif