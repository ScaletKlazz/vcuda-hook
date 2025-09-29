#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <vector>

#include "device/device.hpp"

class Client {
public:

private:
    Client() = default;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    std::vector<Device> devices_;
};

#endif // CLIENT_HPP