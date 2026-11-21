#pragma once
#include "types.hpp"
#include <string_view>

namespace batexe::crypto {

ByteVector encrypt(const Byte* data, size_t size, std::string_view key);
ByteVector decrypt(const Byte* data, size_t size, std::string_view key);

inline ByteVector encrypt(const ByteVector& data, std::string_view key) {
    return encrypt(data.data(), data.size(), key);
}

inline ByteVector decrypt(const ByteVector& data, std::string_view key) {
    return decrypt(data.data(), data.size(), key);
}

std::string generate_random_key(size_t length = 32);

}
