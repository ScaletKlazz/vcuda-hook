#include <dlfcn.h>
#include <cstring>
#include <iostream>

#include "spdlog/spdlog.h"

#include "nvml/nvml_hook.hpp"

extern void* real_dlsym(void* handle, const char* symbol);

