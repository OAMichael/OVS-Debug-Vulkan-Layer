#ifndef UTILS__SIGNATURE_UTILS_H
#define UTILS__SIGNATURE_UTILS_H

#include <CommonUtils.h>
#include <VulkanAPICallIDGenerated.h>

#include <vector>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <memory>

namespace OVS {
namespace SignatureSerializer {

class Allocator {
public:
    template <typename T>
    inline T* Allocate(size_t N = 1) {
        size_t byteSize = N * sizeof(T);
        auto ptr = std::make_unique<uint8_t[]>(byteSize);
        auto& back = allocations_.emplace_back(std::move(ptr));
        return reinterpret_cast<T*>(back.get());
    }

private:
    std::vector<std::unique_ptr<uint8_t[]>> allocations_;
};

struct SignatureHeader {
    APICallID callID{APICallID::Unknown};
    uint16_t thread{0};
    uint32_t byteSize{0};
    uint64_t globalIndex{0};
};

struct BaseSignature {
    SignatureHeader header{};
    Allocator allocator{};

    virtual void SerializeToString(std::stringstream& stream) const = 0;
    virtual void SerializeToJSON(std::stringstream& stream) const = 0;
    virtual void SerializeToStream(WriteStream& stream) const = 0;
    virtual void DeserializeFromStream(const ReadStream& stream) = 0;
    virtual void DeserializeFromJSON(const std::string& json) = 0;
    virtual ~BaseSignature() {}
};

using SignaturePtr = std::unique_ptr<BaseSignature>;

template <typename T, typename U = T>
concept SameOrConstVersion = std::is_same_v<T, U> ||
                             std::is_same_v<T, const U> ||
                             std::is_same_v<std::remove_pointer_t<T>, const std::remove_pointer_t<T>>;


static inline std::string ConvertWideStringToMultibyte(const std::wstring& wstr) {
    std::setlocale(LC_ALL, "en_US.utf8");
    size_t mblen = std::wcstombs(nullptr, wstr.c_str(), 0);
    std::string mbstr(mblen, 0);
    std::wcstombs(mbstr.data(), wstr.c_str(), mblen);
    return mbstr;
}


// Serialize to string
//
template <typename T>
static inline void SerializeToString(const T& value, std::stringstream& stream) {
    if constexpr (std::is_same_v<T, int8_t>) {
        stream << int16_t(value);
    }
    else if constexpr (std::is_same_v<T, uint8_t>) {
        stream << uint16_t(value);
    }
    else if constexpr (std::is_same_v<T, bool>) {
        stream << std::boolalpha << value << std::noboolalpha;
    }
    else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>) {
        if (value) {
            stream << '\"' << value << '\"';
        }
        else {
            stream << "0x0";
        }
    }
    else if constexpr (std::is_same_v<T, const wchar_t*> || std::is_same_v<T, wchar_t*>) {
        if (value) {
            stream << '\"' << ConvertWideStringToMultibyte(value) << '\"';
        }
        else {
            stream << "0x0";
        }
    }
    else if constexpr (std::is_pointer_v<T> || std::is_null_pointer_v<T>) {
        stream << std::hex << "0x" << uint64_t(value) << std::dec;
    }
    else {
        stream << value;
    }
}


// Serialize to stream
//
template <typename T>
static inline void SerializeToStream(const T& value, WriteStream& stream) {
    if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>) {
        if (value) {
            size_t size = std::strlen(value) + 1;
            stream.Write(uint64_t(size));
            stream.Write(value, size * sizeof(char));
        }
        else {
            stream.Write(uint64_t(0));
        }
    }
    else if constexpr (std::is_same_v<T, const wchar_t*> || std::is_same_v<T, wchar_t*>) {
        if (value) {
            size_t size = std::wcslen(value) + 1;
            stream.Write(uint64_t(size));
            stream.Write(value, size * sizeof(wchar_t));
        }
        else {
            stream.Write(uint64_t(0));
        }
    }
    else {
        stream.Write(value);
    }
}

static inline void SerializeToStream(const void* value, size_t size, WriteStream& stream) { stream.Write(value, size); }


// Deserialize from stream
//
template <typename T>
static inline void DeserializeFromStream(T& value, Allocator& allocator, const ReadStream& stream) {
    if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>) {
        uint64_t size = 0;
        stream.Read(size);
        if (size != 0) {
            value = allocator.Allocate<char>(size);
            stream.Read(value, size * sizeof(char));
        }
        else {
            value = nullptr;
        }
    }
    else if constexpr (std::is_same_v<T, const wchar_t*> || std::is_same_v<T, wchar_t*>) {
        uint64_t size = 0;
        stream.Read(size);
        if (size != 0) {
            value = allocator.Allocate<wchar_t>(size);
            stream.Read(value, size * sizeof(wchar_t));
        }
        else {
            value = nullptr;
        }
    }
    else {
        stream.Read(value);
    }
}

static inline void DeserializeFromStream(void*  value, size_t size, Allocator& allocator, const ReadStream& stream) { stream.Read(value, size); }


// DeepCopy
//
template <typename T, typename U = T>
requires SameOrConstVersion<T, U>
static inline void DeepCopy(const T& valueIn, Allocator& allocator, U& valueOut) {
    if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>) {
        if (valueIn) {
            size_t size = std::strlen(valueIn) + 1;
            valueOut = allocator.Allocate<char>(size);
            std::memcpy(valueOut, valueIn, size * sizeof(char));
        }
        else {
            valueOut = nullptr;
        }
    }
    else if constexpr (std::is_same_v<T, const wchar_t*> || std::is_same_v<T, wchar_t*>) {
        if (valueIn) {
            size_t size = std::wcslen(valueIn) + 1;
            valueOut = allocator.Allocate<wchar_t>(size);
            std::memcpy(valueOut, valueIn, size * sizeof(wchar_t));
        }
        else {
            valueOut = nullptr;
        }
    }
    else {
        valueOut = valueIn;
    }
}

static inline void DeepCopy(const void* valueIn, Allocator& allocator, void* valueOut, size_t size) { std::memcpy(valueOut, valueIn, size); }

} // namespace SignatureSerializer
} // namespace OVS

#endif // UTILS__SIGNATURE_UTILS_H