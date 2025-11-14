# vcuda-hook
[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2FScaletKlazz%2Fvcuda-hook.svg?type=shield)](https://app.fossa.com/projects/git%2Bgithub.com%2FScaletKlazz%2Fvcuda-hook?ref=badge_shield)
![CodeQL](https://github.com/ScaletKlazz/vcuda-hook/actions/workflows/codeQL.yml/badge.svg)
![Build](https://github.com/ScaletKlazz/vcuda-hook/actions/workflows/cmake-base-validation.yml/badge.svg)
![Issues](https://img.shields.io/github/issues/ScaletKlazz/vcuda-hook)
![Release](https://img.shields.io/github/v/release/ScaletKlazz/vcuda-hook?display_name=tag)
![License](https://img.shields.io/github/license/ScaletKlazz/vcuda-hook)

a transparent-level library overhook lib-cuda and lib-nvidia-ml

# Build Dependencies
- [CMake](https://cmake.org) >= 3.19
- [Docker](https://www.docker.com) > 20.10
- [CUDA](https://developer.nvidia.com/cuda-zone) >= 12.6
- [Yaml-cpp](https://github.com/jbeder/yaml-cpp) > 0.7
- [Spdlog](https://github.com/gabime/spdlog) > 1.x

# How to Use
## build
1. build builder image
```
bash ./hack/build-builder.sh
```
2. build library
```
bash ./hack/build-via-docker.sh
```
## configure
```
# use env
export LD_PRELOAD=/path/to/libvcuda-hook.so
export VCUDA_LOG_LEVEL=debug
export VCUDA_MEMORY_LIMIT=(1024 * 1024 * 1024 * 10) // limit 10G
```
## usage
```
# manual
your_application

# or use docker
docker run -it --gpus all --rm -v /path/to/libvcuda-hook.so:/usr/lib64/libvcuda-hook.so -e LD_PRELOAD=/usr/lib64/libvcuda-hook.so vllm/vllm-openai:latest bash
```

# Features

## GPU Virtualization Features

### Base Features
- ✅ Minimal Performance Overhead
- ✅ Fractional GPU Usage
- ✅ Fine-grained GPU Memory Control
- ✅ Multi‑Process GPU Memory Unified Control
- ✅ Container GPU Sharing
- ☐ Kubernetes Support
- ...

### More Features
- ☐ Oversub GPU Memory Control
- ☐ GPU Task Hot Snapshot
- ...

### Flow Diagram
```mermaid
graph TD
  A[Application] -->|LD_PRELOAD| B[libvcuda-hook.so]
  B --> C["global init (Logger)"]
  B --> D["dlsym (exported)"]
  D --> S["real_dlsym / underlying dlsym (dlvsym)"]
  D -->|"symbol matches cu*"| E[CudaHook]
  D -->|"symbol matches nvml*"| F[NvmlHook]
  D -->|"other symbols"| G["Original libraries"]

  subgraph Hooking
    E --> H["tryHookSymbol / getHookedSymbol"]
    H -->|"hookInfo.hookedFunc == NO_HOOK or none"| G
    H -->|"hookInfo.hookedFunc != NO_HOOK"| I["replace symbol with hooked function"]
    I --> J["hooked CUDA API wrappers"]
  end

  subgraph CUDA_API_Flow
    J --> K[cuGetProcAddress]
    K -->|"resolve original symbol"| S
    K -->|"if hooked symbol"| I

    J --> M["Memory APIs (cuMemAlloc / cuMemAllocPitch / cuMemAllocManaged / cuMemCreate / ...)"]
    M --> N["Device Memory Manager"]
    N --> O{"Device memory limit set?"}
    O -->|"yes"| P["compare usage + requested -> if exceed -> return CUDA_ERROR_OUT_OF_MEMORY"]
    O -->|"no or ok"| Q["call original API on Original CUDA library"]
    Q --> R["updateMemoryUsage (MemAlloc / MemFree / MemRelease) in Device"]
    R --> T["Client: update shared process metrics (shm)"]

    J --> U["cuMemGetInfo / cuDeviceTotalMem"]
    U -->|"if device limit configured"| V["report virtual total/free = limit * (1 + oversub_ratio)"]
    U -->|"else"| Q
  end

  subgraph Client_Shared_Metrics
    T --> W["shm_open / mmap create_or_attach_process_metric_data"]
    W --> X["store per-process usage entries"]
    X --> Y["get_device_process_metric_data() aggregate across processes"]
  end

  G --> Z["Underlying CUDA driver / GPU"]
  Q --> Z

```


## Why This Project?
Based on several core motivations, I developed this project:

- Personal Technical Interest and Professional Needs: Driven by interest in GPU virtualization technology and CUDA programming, along with related requirements encountered in practical work
- Open Architecture: Provide an open-source solution that allows the community to participate in improvements and feature extensions
- High Scalability: Design a flexible architecture that supports various GPU virtualization scenarios, including GPU resource sharing in containerized environments
- Dynamic Controllability: Implement runtime dynamic configuration and management capabilities, allowing GPU resource allocation adjustments based on demand
- Transparent Proxy Layer: Serve as a transparent proxy for CUDA dynamic libraries, enabling GPU virtualization functionality without modifying existing applications

This project aims to provide a simple and easy-to-use GPU virtualization solution for containerized environments, enabling safe and efficient sharing of GPU resources among multiple containers.

## Contributing
[Code of conduct](/CODE_OF_CONDUCT.md)

## License
[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2FScaletKlazz%2Fvcuda-hook.svg?type=large)](https://app.fossa.com/projects/git%2Bgithub.com%2FScaletKlazz%2Fvcuda-hook?ref=badge_large)