#include "remote/transport.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "spdlog/spdlog.h"

namespace remote {
namespace {

struct WireHeader {
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint8_t reserved;
    uint64_t request_id;
    uint32_t payload_length;
} __attribute__((packed));

bool sendAll(int fd, const void* data, std::size_t size) {
    const auto* ptr = static_cast<const uint8_t*>(data);
    std::size_t sent = 0;
    while (sent < size) {
        const auto rc = ::send(fd, ptr + sent, size - sent, 0);
        if (rc <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(rc);
    }
    return true;
}

bool recvAll(int fd, void* data, std::size_t size) {
    auto* ptr = static_cast<uint8_t*>(data);
    std::size_t received = 0;
    while (received < size) {
        const auto rc = ::recv(fd, ptr + received, size - received, 0);
        if (rc <= 0) {
            return false;
        }
        received += static_cast<std::size_t>(rc);
    }
    return true;
}

bool splitAddress(const std::string& address, std::string& host, std::string& port) {
    const auto pos = address.rfind(':');
    if (pos == std::string::npos || pos == 0 || pos + 1 >= address.size()) {
        return false;
    }
    host = address.substr(0, pos);
    port = address.substr(pos + 1);
    return true;
}

class TcpTransportConnection final : public TransportConnection {
public:
    ~TcpTransportConnection() override {
        close();
    }

    bool connect(const std::string& address, std::size_t timeout_ms) override {
        close();

        std::string host;
        std::string port;
        if (!splitAddress(address, host, port)) {
            spdlog::error("invalid remote address: {}", address);
            return false;
        }

        struct addrinfo hints {};
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family = AF_UNSPEC;

        struct addrinfo* result = nullptr;
        if (::getaddrinfo(host.c_str(), port.c_str(), &hints, &result) != 0) {
            spdlog::error("getaddrinfo failed for {}", address);
            return false;
        }

        for (auto* rp = result; rp != nullptr; rp = rp->ai_next) {
            fd_ = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (fd_ < 0) {
                continue;
            }

            ::fcntl(fd_, F_SETFL, O_NONBLOCK);
            const int rc = ::connect(fd_, rp->ai_addr, static_cast<socklen_t>(rp->ai_addrlen));
            if (rc == 0) {
                ::fcntl(fd_, F_SETFL, 0);
                ::freeaddrinfo(result);
                return true;
            }

            if (errno == EINPROGRESS) {
                fd_set wfds;
                FD_ZERO(&wfds);
                FD_SET(fd_, &wfds);
                struct timeval tv {};
                tv.tv_sec = static_cast<long>(timeout_ms / 1000);
                tv.tv_usec = static_cast<long>((timeout_ms % 1000) * 1000);
                const auto selected = ::select(fd_ + 1, nullptr, &wfds, nullptr, &tv);
                if (selected > 0 && FD_ISSET(fd_, &wfds)) {
                    int so_error = 0;
                    socklen_t len = sizeof(so_error);
                    ::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &so_error, &len);
                    if (so_error == 0) {
                        ::fcntl(fd_, F_SETFL, 0);
                        ::freeaddrinfo(result);
                        return true;
                    }
                }
            }

            ::close(fd_);
            fd_ = -1;
        }

        ::freeaddrinfo(result);
        return false;
    }

    void close() override {
        if (fd_ >= 0) {
            ::shutdown(fd_, SHUT_RDWR);
            ::close(fd_);
            fd_ = -1;
        }
    }

    bool isConnected() const override {
        return fd_ >= 0;
    }

    bool sendFrame(const FrameHeader& header, const std::vector<uint8_t>& payload) override {
        if (!isConnected()) {
            return false;
        }
        WireHeader wire{
            header.version,
            static_cast<uint8_t>(header.type),
            header.flags,
            0,
            header.request_id,
            header.payload_length,
        };
        return sendAll(fd_, &wire, sizeof(wire)) &&
               (payload.empty() || sendAll(fd_, payload.data(), payload.size()));
    }

    bool receiveFrame(FrameHeader& header, std::vector<uint8_t>& payload) override {
        if (!isConnected()) {
            return false;
        }
        WireHeader wire{};
        if (!recvAll(fd_, &wire, sizeof(wire))) {
            return false;
        }
        header.version = wire.version;
        header.type = static_cast<MessageType>(wire.type);
        header.flags = wire.flags;
        header.request_id = wire.request_id;
        header.payload_length = wire.payload_length;
        payload.resize(header.payload_length);
        return payload.empty() || recvAll(fd_, payload.data(), payload.size());
    }

private:
    int fd_ = -1;
};

class UcxTransportConnection final : public TransportConnection {
public:
    bool connect(const std::string&, std::size_t) override {
        spdlog::warn("UCX transport is not built in this environment, falling back to TCP");
        return false;
    }

    void close() override {}

    bool isConnected() const override {
        return false;
    }

    bool sendFrame(const FrameHeader&, const std::vector<uint8_t>&) override {
        return false;
    }

    bool receiveFrame(FrameHeader&, std::vector<uint8_t>&) override {
        return false;
    }
};

}  // namespace

std::unique_ptr<TransportConnection> createTransportConnection(bool prefer_rdma) {
    if (prefer_rdma) {
        spdlog::warn("UCX transport is not available in this build, falling back to TCP transport");
    }
    return std::make_unique<TcpTransportConnection>();
}

}  // namespace remote
