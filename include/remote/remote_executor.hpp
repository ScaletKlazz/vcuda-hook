#ifndef VCUDA_REMOTE_EXECUTOR_HPP
#define VCUDA_REMOTE_EXECUTOR_HPP

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "remote/protocol.hpp"
#include "remote/transport.hpp"

namespace remote {

class RemoteExecutor {
public:
    static RemoteExecutor& instance();

    bool enabled() const;
    bool ensureConnected(std::string* error = nullptr);
    void disconnect();

    DispatchResult dispatch(const std::vector<uint8_t>& payload, const DispatchOptions& options);

    std::optional<HandshakeInfo> handshakeInfo() const;
    int currentDevice() const;
    void setCurrentDevice(int device);
    RemoteMemoryInfo memoryInfo(int device) const;
    std::string deviceName() const;
    bool refreshMemoryInfo();

private:
    RemoteExecutor() = default;

    bool performHandshake(std::string* error);

    mutable std::mutex mutex_;
    std::unique_ptr<TransportConnection> connection_;
    uint64_t next_request_id_ = 1;
    int current_device_ = 0;
    std::optional<HandshakeInfo> handshake_;
};

}  // namespace remote

#endif  // VCUDA_REMOTE_EXECUTOR_HPP
