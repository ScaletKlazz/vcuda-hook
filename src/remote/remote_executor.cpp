#include "remote/remote_executor.hpp"

#include <unistd.h>

#include "remote/buffer.hpp"
#include "spdlog/spdlog.h"
#include "util/config.hpp"

namespace remote {

RemoteExecutor& RemoteExecutor::instance() {
    static RemoteExecutor executor;
    return executor;
}

bool RemoteExecutor::enabled() const {
    return util::Config::executionMode() == util::Config::ExecutionMode::Remote;
}

bool RemoteExecutor::ensureConnected(std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled()) {
        if (error) {
            *error = "remote mode disabled";
        }
        return false;
    }

    if (connection_ && connection_->isConnected() && handshake_) {
        return true;
    }

    connection_ = createTransportConnection(util::Config::remoteRdmaPreferred());
    const auto address = util::Config::remoteAddress();
    if (address.empty()) {
        if (error) {
            *error = "VCUDA_REMOTE_ADDR is empty";
        }
        return false;
    }

    if (!connection_->connect(address, util::Config::remoteTimeoutMs())) {
        if (error) {
            *error = "failed to connect remote server";
        }
        connection_.reset();
        return false;
    }

    return performHandshake(error);
}

void RemoteExecutor::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connection_) {
        connection_->close();
    }
    connection_.reset();
    handshake_.reset();
}

DispatchResult RemoteExecutor::dispatch(const std::vector<uint8_t>& payload, const DispatchOptions& options) {
    std::string error;
    if (!ensureConnected(&error)) {
        return {-1, {}};
    }

    std::lock_guard<std::mutex> lock(mutex_);
    FrameHeader header{};
    header.version = 1;
    header.type = MessageType::ModuleCallRequest;
    header.flags = options.sync ? 1 : 0;
    header.request_id = options.sync ? next_request_id_++ : 0;
    header.payload_length = static_cast<uint32_t>(payload.size());

    if (!connection_->sendFrame(header, payload)) {
        connection_->close();
        return {-1, {}};
    }

    if (!options.sync) {
        return {0, {}};
    }

    FrameHeader response_header{};
    std::vector<uint8_t> response_payload;
    if (!connection_->receiveFrame(response_header, response_payload)) {
        connection_->close();
        return {-1, {}};
    }
    if (response_header.type != MessageType::ModuleCallResponse || response_header.request_id != header.request_id) {
        return {-1, {}};
    }
    return {0, response_payload};
}

std::optional<HandshakeInfo> RemoteExecutor::handshakeInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return handshake_;
}

int RemoteExecutor::currentDevice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_device_;
}

void RemoteExecutor::setCurrentDevice(int device) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_device_ = device;
    if (handshake_ && device >= 0 && device < handshake_->device_count) {
        handshake_->current_device = device;
    }
}

RemoteMemoryInfo RemoteExecutor::memoryInfo(int device) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!handshake_ || device < 0 || device >= static_cast<int>(handshake_->devices.size())) {
        return {};
    }
    return handshake_->devices[static_cast<std::size_t>(device)];
}

std::string RemoteExecutor::deviceName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!handshake_) {
        return {};
    }
    return handshake_->device_name;
}

bool RemoteExecutor::performHandshake(std::string* error) {
    BufferWriter writer;
    writer.addString("vcuda-hook");
    writer.add(static_cast<uint32_t>(::getpid()));
    writer.addString("default-host");

    FrameHeader header{};
    header.version = 1;
    header.type = MessageType::HandshakeRequest;
    header.request_id = next_request_id_++;
    header.payload_length = static_cast<uint32_t>(writer.bytes().size());

    if (!connection_->sendFrame(header, writer.bytes())) {
        if (error) {
            *error = "handshake send failed";
        }
        return false;
    }

    FrameHeader response_header{};
    std::vector<uint8_t> response_payload;
    if (!connection_->receiveFrame(response_header, response_payload)) {
        if (error) {
            *error = "handshake receive failed";
        }
        return false;
    }
    if (response_header.type != MessageType::HandshakeResponse) {
        if (error) {
            *error = "invalid handshake response";
        }
        return false;
    }

    BufferReader reader(response_payload);
    HandshakeInfo info;
    info.device_count = reader.read<int32_t>();
    info.current_device = reader.read<int32_t>();
    info.device_name = reader.readString();
    const auto mem_count = reader.read<uint32_t>();
    info.devices.reserve(mem_count);
    for (uint32_t i = 0; i < mem_count; ++i) {
        info.devices.push_back({
            reader.read<std::size_t>(),
            reader.read<std::size_t>(),
        });
    }

    current_device_ = info.current_device;
    handshake_ = info;
    return true;
}

bool RemoteExecutor::refreshMemoryInfo() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!handshake_ || !connection_ || !connection_->isConnected()) {
        return false;
    }

    std::vector<uint8_t> payload;
    const auto module = static_cast<uint8_t>(ModuleId::Internal);
    const std::string symbol = "interGetVDeviceMemoryInfo";
    const auto symbol_size = symbol.size();
    payload.insert(payload.end(), reinterpret_cast<const uint8_t*>(&module),
                   reinterpret_cast<const uint8_t*>(&module) + sizeof(module));
    payload.insert(payload.end(), reinterpret_cast<const uint8_t*>(&symbol_size),
                   reinterpret_cast<const uint8_t*>(&symbol_size) + sizeof(symbol_size));
    payload.insert(payload.end(), symbol.begin(), symbol.end());

    FrameHeader header{};
    header.version = 1;
    header.type = MessageType::ModuleCallRequest;
    header.flags = 1;
    header.request_id = next_request_id_++;
    header.payload_length = static_cast<uint32_t>(payload.size());
    if (!connection_->sendFrame(header, payload)) {
        return false;
    }

    FrameHeader response_header{};
    std::vector<uint8_t> response_payload;
    if (!connection_->receiveFrame(response_header, response_payload) ||
        response_header.type != MessageType::ModuleCallResponse ||
        response_header.request_id != header.request_id) {
        return false;
    }

    BufferReader reader(response_payload);
    auto& devices = handshake_->devices;
    for (auto& device : devices) {
        device.total = reader.read<std::size_t>();
        device.free = reader.read<std::size_t>();
    }
    return true;
}

}  // namespace remote
