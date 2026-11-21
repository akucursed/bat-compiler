#include "crypto.hpp"
#include <windows.h>
#include <random>

namespace batexe::crypto {

namespace {

void rc4_init(uint8_t* s, const uint8_t* key, size_t key_len) {
    for (int i = 0; i < 256; ++i) {
        s[i] = static_cast<uint8_t>(i);
    }
    int j = 0;
    for (int i = 0; i < 256; ++i) {
        j = (j + s[i] + key[i % key_len]) & 0xFF;
        std::swap(s[i], s[j]);
    }
}

void rc4_crypt(uint8_t* s, const uint8_t* in, uint8_t* out, size_t len) {
    int i = 0;
    int j = 0;
    for (size_t n = 0; n < len; ++n) {
        i = (i + 1) & 0xFF;
        j = (j + s[i]) & 0xFF;
        std::swap(s[i], s[j]);
        int k = (s[i] + s[j]) & 0xFF;
        out[n] = in[n] ^ s[k];
    }
}

}

ByteVector encrypt(const Byte* data, size_t size, std::string_view key) {
    if (!data || size == 0) return {};

    std::string_view effective_key = key.empty() ? "syn_bat_vm_key_default" : key;

    uint8_t s[256];
    rc4_init(s, reinterpret_cast<const uint8_t*>(effective_key.data()), effective_key.size());

    ByteVector output(size);
    rc4_crypt(s, data, output.data(), size);
    return output;
}

ByteVector decrypt(const Byte* data, size_t size, std::string_view key) {
    return encrypt(data, size, key);
}

std::string generate_random_key(size_t length) {
    static const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!@#$%^&*()";
    std::random_device rd;
    std::mt19937 gen(rd() ^ static_cast<unsigned int>(GetTickCount64()));
    std::uniform_int_distribution<size_t> dis(0, sizeof(chars) - 2);

    std::string key;
    key.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        key += chars[dis(gen)];
    }
    return key;
}

}
