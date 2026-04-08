#ifndef VCUDA_REMOTE_PROTOCOL_HPP
#define VCUDA_REMOTE_PROTOCOL_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace remote {

enum class ModuleId : uint8_t {
    Internal = 0,
    Cuda = 1,
    Nvml = 10,
};

enum class MessageType : uint8_t {
    HandshakeRequest = 1,
    HandshakeResponse = 2,
    ModuleCallRequest = 3,
    ModuleCallResponse = 4,
    ErrorResponse = 5,
};

enum class ResponseStatus : uint8_t {
    Ok = 0,
    Error = 1,
};

struct FrameHeader {
    uint8_t version = 1;
    MessageType type = MessageType::ErrorResponse;
    uint8_t flags = 0;
    uint64_t request_id = 0;
    uint32_t payload_length = 0;
};

struct RemoteMemoryInfo {
    std::size_t total = 0;
    std::size_t free = 0;
};

struct HandshakeInfo {
    int device_count = 1;
    int current_device = 0;
    std::string device_name;
    std::vector<RemoteMemoryInfo> devices;
};

struct DispatchOptions {
    ModuleId module = ModuleId::Cuda;
    bool sync = true;
};

struct DispatchResult {
    int status_code = 0;
    std::vector<uint8_t> payload;

    bool ok() const {
        return status_code == 0;
    }
};

}  // namespace remote

#endif  // VCUDA_REMOTE_PROTOCOL_HPP
