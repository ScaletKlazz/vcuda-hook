#include "backend/remote_backend.hpp"

#include <cstring>
#include <type_traits>

#include "backend/local_backend.hpp"
#include "cuda/cuda_hook.hpp"
#include "nvml/nvml_hook.hpp"
#include "remote/buffer.hpp"
#include "remote/remote_executor.hpp"
#include "spdlog/spdlog.h"

namespace {

template <typename T>
void appendScalar(std::vector<uint8_t>& out, const T& value) {
    static_assert(std::is_trivially_copyable<T>::value, "scalar must be trivially copyable");
    const auto* ptr = reinterpret_cast<const uint8_t*>(&value);
    out.insert(out.end(), ptr, ptr + sizeof(T));
}

void appendSymbol(std::vector<uint8_t>& out, remote::ModuleId module, const std::string& symbol) {
    const auto module_raw = static_cast<uint8_t>(module);
    appendScalar(out, module_raw);
    const auto size = symbol.size();
    appendScalar(out, size);
    out.insert(out.end(), symbol.begin(), symbol.end());
}

template <typename T>
bool readScalar(remote::BufferReader& reader, T& out) {
    try {
        out = reader.read<T>();
        return true;
    } catch (const std::exception& e) {
        spdlog::error("remote response parse failed: {}", e.what());
        return false;
    }
}

}  // namespace

RemoteBackend& RemoteBackend::instance() {
    static RemoteBackend backend;
    return backend;
}

CUresult RemoteBackend::cuMemAlloc(CudaHook& hook, CUdeviceptr* dptr, size_t byteSize) {
    std::vector<uint8_t> payload;
    appendSymbol(payload, remote::ModuleId::Cuda, "cuMemAlloc");
    appendScalar(payload, byteSize);
    const auto result = remote::RemoteExecutor::instance().dispatch(payload, {remote::ModuleId::Cuda, true});
    if (!result.ok()) {
        return CUDA_ERROR_UNKNOWN;
    }
    remote::BufferReader reader(result.payload);
    CUresult status = CUDA_ERROR_UNKNOWN;
    if (!readScalar(reader, status)) {
        return CUDA_ERROR_UNKNOWN;
    }
    if (status == CUDA_SUCCESS && !readScalar(reader, *dptr)) {
        return CUDA_ERROR_UNKNOWN;
    }
    if (status == CUDA_SUCCESS) {
        hook.getDevice().updateMemoryUsage(MemAlloc, *dptr, byteSize);
        remote::RemoteExecutor::instance().refreshMemoryInfo();
    }
    return status;
}

CUresult RemoteBackend::cuMemFree(CudaHook& hook, CUdeviceptr dptr) {
    std::vector<uint8_t> payload;
    appendSymbol(payload, remote::ModuleId::Cuda, "cuMemFree");
    appendScalar(payload, static_cast<uint64_t>(dptr));
    const auto result = remote::RemoteExecutor::instance().dispatch(payload, {remote::ModuleId::Cuda, true});
    if (!result.ok()) {
        return CUDA_ERROR_UNKNOWN;
    }
    remote::BufferReader reader(result.payload);
    CUresult status = CUDA_ERROR_UNKNOWN;
    if (!readScalar(reader, status)) {
        return CUDA_ERROR_UNKNOWN;
    }
    if (status == CUDA_SUCCESS) {
        hook.getDevice().updateMemoryUsage(MemFree, dptr);
        remote::RemoteExecutor::instance().refreshMemoryInfo();
    }
    return status;
}

CUresult RemoteBackend::cuCtxGetDevice(CudaHook& hook, CUdevice* device) {
    std::string error;
    if (!remote::RemoteExecutor::instance().ensureConnected(&error)) {
        spdlog::error("remote cuCtxGetDevice failed: {}", error);
        return CUDA_ERROR_NOT_INITIALIZED;
    }
    const auto local = LocalBackend::instance().cuCtxGetDevice(hook, device);
    if (local == CUDA_SUCCESS) {
        remote::RemoteExecutor::instance().setCurrentDevice(int(*device));
        return local;
    }
    *device = remote::RemoteExecutor::instance().currentDevice();
    hook.getDevice().setDeviceId(int(*device));
    return CUDA_SUCCESS;
}

CUresult RemoteBackend::cuCtxSetCurrent(CudaHook& hook, CUcontext ctx) {
    int device = hook.getDevice().getDeviceId();
    const auto local = LocalBackend::instance().cuCtxSetCurrent(hook, ctx);
    if (local == CUDA_SUCCESS) {
        device = hook.getDevice().getDeviceId();
    }

    std::vector<uint8_t> payload;
    appendSymbol(payload, remote::ModuleId::Cuda, "cuCtxSetCurrent");
    appendScalar(payload, device);
    const auto result = remote::RemoteExecutor::instance().dispatch(payload, {remote::ModuleId::Cuda, true});
    if (!result.ok()) {
        return CUDA_ERROR_UNKNOWN;
    }

    remote::BufferReader reader(result.payload);
    CUresult status = CUDA_ERROR_UNKNOWN;
    if (!readScalar(reader, status)) {
        return CUDA_ERROR_UNKNOWN;
    }
    if (status == CUDA_SUCCESS) {
        hook.getDevice().setDeviceId(device);
        remote::RemoteExecutor::instance().setCurrentDevice(device);
    }
    return status;
}

CUresult RemoteBackend::cuMemGetInfo(CudaHook&, size_t* free, size_t* total) {
    std::string error;
    if (!remote::RemoteExecutor::instance().ensureConnected(&error)) {
        spdlog::error("remote cuMemGetInfo failed: {}", error);
        return CUDA_ERROR_NOT_INITIALIZED;
    }
    const auto info = remote::RemoteExecutor::instance().memoryInfo(remote::RemoteExecutor::instance().currentDevice());
    *free = info.free;
    *total = info.total;
    return CUDA_SUCCESS;
}

CUresult RemoteBackend::cuDeviceTotalMem(CudaHook&, size_t* bytes, CUdevice dev) {
    std::string error;
    if (!remote::RemoteExecutor::instance().ensureConnected(&error)) {
        spdlog::error("remote cuDeviceTotalMem failed: {}", error);
        return CUDA_ERROR_NOT_INITIALIZED;
    }
    *bytes = remote::RemoteExecutor::instance().memoryInfo(int(dev)).total;
    return CUDA_SUCCESS;
}

nvmlReturn_t RemoteBackend::nvmlDeviceGetMemoryInfo(NvmlHook&, nvmlDevice_t, nvmlMemory_t* memory) {
    std::string error;
    if (!remote::RemoteExecutor::instance().ensureConnected(&error)) {
        spdlog::error("remote nvmlDeviceGetMemoryInfo failed: {}", error);
        return NVML_ERROR_UNINITIALIZED;
    }
    const auto info = remote::RemoteExecutor::instance().memoryInfo(remote::RemoteExecutor::instance().currentDevice());
    memory->total = info.total;
    memory->free = info.free;
    memory->used = info.total - info.free;
    return NVML_SUCCESS;
}

nvmlReturn_t RemoteBackend::nvmlDeviceGetMemoryInfo_v2(NvmlHook&, nvmlDevice_t, nvmlMemory_v2_t* memory) {
    std::string error;
    if (!remote::RemoteExecutor::instance().ensureConnected(&error)) {
        spdlog::error("remote nvmlDeviceGetMemoryInfo_v2 failed: {}", error);
        return NVML_ERROR_UNINITIALIZED;
    }
    const auto info = remote::RemoteExecutor::instance().memoryInfo(remote::RemoteExecutor::instance().currentDevice());
    memory->total = info.total;
    memory->free = info.free;
    memory->used = info.total - info.free;
    memory->reserved = 0;
    return NVML_SUCCESS;
}

nvmlReturn_t RemoteBackend::nvmlDeviceGetName(NvmlHook&, nvmlDevice_t, char* name, unsigned int length) {
    std::string error;
    if (!remote::RemoteExecutor::instance().ensureConnected(&error)) {
        spdlog::error("remote nvmlDeviceGetName failed: {}", error);
        return NVML_ERROR_UNINITIALIZED;
    }
    const auto remote_name = remote::RemoteExecutor::instance().deviceName();
    if (remote_name.empty()) {
        return NVML_ERROR_UNINITIALIZED;
    }
    std::strncpy(name, remote_name.c_str(), length);
    if (length > 0) {
        name[length - 1] = '\0';
    }
    return NVML_SUCCESS;
}
