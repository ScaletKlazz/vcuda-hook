#ifndef VCUDA_REMOTE_TRANSPORT_HPP
#define VCUDA_REMOTE_TRANSPORT_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "remote/protocol.hpp"

namespace remote {

class TransportConnection {
public:
    virtual ~TransportConnection() = default;

    virtual bool connect(const std::string& address, std::size_t timeout_ms) = 0;
    virtual void close() = 0;
    virtual bool isConnected() const = 0;
    virtual bool sendFrame(const FrameHeader& header, const std::vector<uint8_t>& payload) = 0;
    virtual bool receiveFrame(FrameHeader& header, std::vector<uint8_t>& payload) = 0;
};

std::unique_ptr<TransportConnection> createTransportConnection(bool prefer_rdma);

}  // namespace remote

#endif  // VCUDA_REMOTE_TRANSPORT_HPP
