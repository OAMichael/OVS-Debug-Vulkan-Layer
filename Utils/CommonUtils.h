#ifndef UTILS__COMMON_UTILS_H
#define UTILS__COMMON_UTILS_H

#include <vector>
#include <cstdint>

namespace OVS {

class WriteStream {
public:
    explicit WriteStream(std::vector<uint8_t>& d) : data_(d) {}

    inline void Write(const void* pdata, size_t bytesize) {
        auto size = data_.size();
        data_.resize(size + bytesize);
        std::memcpy(data_.data() + size, pdata, bytesize);
    }

    template <typename T>
    inline void Write(const T& e) {
        Write(&e, sizeof(T));
    }

    inline size_t Size() const { return data_.size(); }

private:
    std::vector<uint8_t>& data_;
};

class ReadStream {
public:
    explicit ReadStream(const std::vector<uint8_t>& d) : data_(d) {}

    inline bool Read(void* pdata, size_t bytesize) const {
        auto size = data_.size();
        if (size < offset_ + bytesize) {
            return false;
        }
        std::memcpy(pdata, data_.data() + offset_, bytesize);
        offset_ += bytesize;
        return true;
    }

    inline bool ReadNoAdvance(void* pdata, size_t bytesize) const {
        auto size = data_.size();
        if (size < offset_ + bytesize) {
            return false;
        }
        std::memcpy(pdata, data_.data() + offset_, bytesize);
        return true;
    }

    template <typename T>
    inline bool Read(T& e) const {
        return Read(&e, sizeof(T));
    }

    template <typename T>
    inline bool ReadNoAdvance(T& e) const {
        return ReadNoAdvance(&e, sizeof(T));
    }

private:
    const std::vector<uint8_t>& data_;
    mutable size_t offset_{0};
};

} // namespace OVS

#endif // UTILS__COMMON_UTILS_H