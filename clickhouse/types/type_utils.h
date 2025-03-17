#pragma once

#include "types.h"
#include "clickhouse/exceptions.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

namespace clickhouse {

// Keep the new_int formatting API while using upstream's selectable Int128 backend.
inline std::string ToString(const Int128& value) { return Bignum::Int128ToString(value); }
inline std::string ToString(const UInt128& value) { return Bignum::UInt128ToString(value); }

namespace detail {
inline void Negate256(unsigned char* bytes) {
    unsigned carry = 1;
    for (size_t i = 0; i < 32; ++i) {
        carry += static_cast<unsigned char>(~bytes[i]);
        bytes[i] = static_cast<unsigned char>(carry);
        carry >>= 8;
    }
}

inline std::string FormatUnsigned256(const unsigned char* source) {
    unsigned char bytes[32];
    std::memcpy(bytes, source, sizeof(bytes));
    std::string result;
    bool more;
    do {
        unsigned remainder = 0;
        more = false;
        for (size_t i = 32; i-- > 0;) {
            unsigned value = remainder * 256 + bytes[i];
            bytes[i] = static_cast<unsigned char>(value / 10);
            remainder = value % 10;
            more |= bytes[i] != 0;
        }
        result.push_back(static_cast<char>('0' + remainder));
    } while (more);
    std::reverse(result.begin(), result.end());
    return result;
}

inline UInt256 ParseUnsigned256(std::string_view value) {
    if (value.empty()) throw ValidationError("Empty 256-bit integer");
    UInt256 result{};
    for (char c : value) {
        if (c < '0' || c > '9') throw ValidationError("Invalid 256-bit integer");
        unsigned carry = static_cast<unsigned>(c - '0');
        for (auto& byte : result.bytes) {
            carry += static_cast<unsigned>(byte) * 10;
            byte = static_cast<unsigned char>(carry);
            carry >>= 8;
        }
        if (carry) throw ValidationError("256-bit integer overflow");
    }
    return result;
}
} // namespace detail

inline std::string ToString(const UInt256& value) {
    return detail::FormatUnsigned256(value.bytes);
}
inline std::string ToString(const Int256& value) {
    if (!(value.bytes[31] & 0x80)) return detail::FormatUnsigned256(value.bytes);
    Int256 magnitude = value;
    detail::Negate256(magnitude.bytes);
    return "-" + detail::FormatUnsigned256(magnitude.bytes);
}

inline UInt256 UInt256FromString(std::string_view value) {
    return detail::ParseUnsigned256(value);
}
inline Int256 Int256FromString(std::string_view value) {
    bool negative = !value.empty() && value.front() == '-';
    if (negative) value.remove_prefix(1);
    const auto magnitude = detail::ParseUnsigned256(value);
    if (magnitude.bytes[31] & 0x80) {
        bool minimum = negative && magnitude.bytes[31] == 0x80;
        for (size_t i = 0; i < 31; ++i) minimum &= magnitude.bytes[i] == 0;
        if (!minimum) throw ValidationError("Signed 256-bit integer overflow");
    }
    Int256 result;
    std::memcpy(result.bytes, magnitude.bytes, sizeof(result.bytes));
    if (negative) detail::Negate256(result.bytes);
    return result;
}

inline Int256 Int256FromInt128(const Int128& value) {
    Int256 result;
    auto low = Bignum::Int128Low64(value);
    auto high = static_cast<uint64_t>(Bignum::Int128High64(value));
    std::memset(result.bytes, (high >> 63) ? 0xFF : 0, sizeof(result.bytes));
    for (size_t i = 0; i < 8; ++i) {
        result.bytes[i] = static_cast<unsigned char>(low >> (8 * i));
        result.bytes[i + 8] = static_cast<unsigned char>(high >> (8 * i));
    }
    return result;
}
inline Int128 Int256ToInt128(const Int256& value) {
    const unsigned char sign = value.bytes[15] & 0x80 ? 0xFF : 0;
    for (size_t i = 16; i < 32; ++i) {
        if (value.bytes[i] != sign) throw ValidationError("Decimal256 value exceeds Int128; use At256() or StringAt()");
    }
    uint64_t low = 0, high = 0;
    for (size_t i = 0; i < 8; ++i) {
        low |= static_cast<uint64_t>(value.bytes[i]) << (8 * i);
        high |= static_cast<uint64_t>(value.bytes[i + 8]) << (8 * i);
    }
    return Bignum::MakeInt128(static_cast<int64_t>(high), low);
}

} // namespace clickhouse
