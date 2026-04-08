#ifndef VCUDA_REMOTE_BUFFER_HPP
#define VCUDA_REMOTE_BUFFER_HPP

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace remote {

class BufferWriter {
public:
    template <typename T>
    void add(const T& value) {
        static_assert(std::is_trivially_copyable<T>::value, "buffer type must be trivially copyable");
        const auto* ptr = reinterpret_cast<const uint8_t*>(&value);
        bytes_.insert(bytes_.end(), ptr, ptr + sizeof(T));
    }

    void addString(const std::string& value) {
        const uint32_t size = static_cast<uint32_t>(value.size());
        add(size);
        bytes_.insert(bytes_.end(), value.begin(), value.end());
    }

    void addBytes(const void* data, std::size_t size) {
        const uint32_t length = static_cast<uint32_t>(size);
        add(length);
        const auto* ptr = reinterpret_cast<const uint8_t*>(data);
        bytes_.insert(bytes_.end(), ptr, ptr + size);
    }

    const std::vector<uint8_t>& bytes() const {
        return bytes_;
    }

private:
    std::vector<uint8_t> bytes_;
};

class BufferReader {
public:
    explicit BufferReader(const std::vector<uint8_t>& bytes) : bytes_(bytes) {}

    template <typename T>
    T read() {
        static_assert(std::is_trivially_copyable<T>::value, "buffer type must be trivially copyable");
        if (offset_ + sizeof(T) > bytes_.size()) {
            throw std::runtime_error("buffer underflow");
        }
        T value{};
        std::memcpy(&value, bytes_.data() + offset_, sizeof(T));
        offset_ += sizeof(T);
        return value;
    }

    std::string readString() {
        const auto size = read<uint32_t>();
        if (offset_ + size > bytes_.size()) {
            throw std::runtime_error("buffer underflow");
        }
        std::string value(reinterpret_cast<const char*>(bytes_.data() + offset_), size);
        offset_ += size;
        return value;
    }

    std::vector<uint8_t> readBytes() {
        const auto size = read<uint32_t>();
        if (offset_ + size > bytes_.size()) {
            throw std::runtime_error("buffer underflow");
        }
        std::vector<uint8_t> out(bytes_.begin() + static_cast<long long>(offset_),
                                 bytes_.begin() + static_cast<long long>(offset_ + size));
        offset_ += size;
        return out;
    }

private:
    const std::vector<uint8_t>& bytes_;
    std::size_t offset_ = 0;
};

}  // namespace remote

#endif  // VCUDA_REMOTE_BUFFER_HPP
