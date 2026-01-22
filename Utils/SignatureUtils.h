#ifndef UTILS__SIGNATURE_UTILS_H
#define UTILS__SIGNATURE_UTILS_H

#include <VulkanAPICallIDGenerated.h>

#include <vector>
#include <cstdint>
#include <sstream>
#include <iomanip>

namespace OVS {
namespace SignatureSerializer {

class WriteStream {
public:
    explicit WriteStream(std::vector<uint8_t>& d) : data_(d) {}

    inline void Write(const void* pdata, size_t bytesize) {
        auto size = data_.size();
        data_.resize(size + bytesize);
        std::memcpy(data_.data() + size, pdata, bytesize);
    }

    inline void Write(const void* pdata, size_t bytesize, size_t offset) {
        auto size = data_.size();
        if (size < offset + bytesize) {
            data_.resize(offset + bytesize);
        }
        std::memcpy(data_.data() + offset, pdata, bytesize);
    }

    template <typename T>
    inline void Write(const T& e) {
        Write(&e, sizeof(T));
    }

    template <typename T>
    inline void Write(const T& e, size_t offset) {
        Write(&e, sizeof(T), offset);
    }

private:
    std::vector<uint8_t>& data_;
};

class ReadStream {
public:
    explicit ReadStream(const std::vector<uint8_t>& d) : data_(d) {}

    inline bool Read(void* pdata, size_t bytesize, size_t offset = 0) const {
        auto size = data_.size();
        if (size < offset + bytesize) {
            return false;
        }
        std::memcpy(pdata, data_.data() + offset, bytesize);
        return true;
    }

    template <typename T>
    inline bool Read(T& e, size_t offset = 0) const {
        return Read(&e, sizeof(T), offset);
    }

private:
    const std::vector<uint8_t>& data_;
};

struct SignatureHeader {
    APICallID callID{APICallID::Unknown};
    uint64_t globalIndex{0};
    uint16_t thread{0};
};

static inline void SerializeToString(signed char        value, std::stringstream& stream) { stream << int16_t(value); }
static inline void SerializeToString(unsigned char      value, std::stringstream& stream) { stream << uint16_t(value); }
static inline void SerializeToString(short              value, std::stringstream& stream) { stream << value; }
static inline void SerializeToString(unsigned short     value, std::stringstream& stream) { stream << value; }
static inline void SerializeToString(int                value, std::stringstream& stream) { stream << value; }
static inline void SerializeToString(unsigned int       value, std::stringstream& stream) { stream << value; }
static inline void SerializeToString(long               value, std::stringstream& stream) { stream << value; }
static inline void SerializeToString(long long          value, std::stringstream& stream) { stream << value; }
static inline void SerializeToString(unsigned long      value, std::stringstream& stream) { stream << value; }
static inline void SerializeToString(unsigned long long value, std::stringstream& stream) { stream << value; }
static inline void SerializeToString(float              value, std::stringstream& stream) { stream << value; }
static inline void SerializeToString(double             value, std::stringstream& stream) { stream << value; }
static inline void SerializeToString(char               value, std::stringstream& stream) { stream << value; }
static inline void SerializeToString(bool               value, std::stringstream& stream) { stream << std::boolalpha << value << std::noboolalpha; }
static inline void SerializeToString(const void*        value, std::stringstream& stream) { stream << std::hex << "0x" << uint64_t(value) << std::dec; }
static inline void SerializeToString(nullptr_t          value, std::stringstream& stream) { stream << "0x0"; }

static inline void SerializeToString(const char*        value, std::stringstream& stream) {
    if (value) {
        stream << '\"' << value << '\"';
    }
    else {
        stream << "0x0";
    }
}

static inline void SerializeToString(const wchar_t*     value, std::stringstream& stream) { /*TODO*/; }

} // namespace SignatureSerializer
} // namespace OVS

#endif // UTILS__SIGNATURE_UTILS_H