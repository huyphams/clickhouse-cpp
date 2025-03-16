#pragma once

#include "types.h"
#include <absl/numeric/int128.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <limits>

namespace clickhouse {

// Convert Int128 to string representation
inline std::string ToString(const Int128& value) {
    std::stringstream ss;

    // Special handling for minimum value since -min == min for two's complement
    if (value == std::numeric_limits<Int128>::min()) {
        // Handle minimum value by converting it digit by digit
        // Get absolute value by parts to avoid overflow
        uint64_t high = absl::Int128High64(value);
        uint64_t low = absl::Int128Low64(value);

        // Convert to positive parts and handle manually
        high = ~high;  // Bitwise NOT for the high part
        low = ~low + 1;  // Add 1 to complete two's complement
        if (low == 0) {  // Handle carry
            high += 1;
        }

        // Now we have the magnitude in high:low parts
        UInt128 magnitude = absl::MakeUint128(high, low);
        ss << "-" << magnitude;
    } else if (value < 0) {
        ss << "-" << -value;
    } else {
        ss << value;
    }
    return ss.str();
}

// Convert UInt128 to string representation
inline std::string ToString(const UInt128& value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

// Convert Int128 to hexadecimal string representation
inline std::string ToHexString(const Int128& value) {
    std::stringstream ss;
    ss << "0x" << std::hex << std::uppercase;

    // Get high and low 64-bit parts
    uint64_t high = absl::Int128High64(value);
    uint64_t low = absl::Int128Low64(value);

    // Print high part only if non-zero
    if (high != 0 || value < 0) {
        ss << std::setfill('0') << std::setw(16) << high;
    }
    ss << std::setfill('0') << std::setw(16) << low;

    return ss.str();
}

// Convert UInt128 to hexadecimal string representation
inline std::string ToHexString(const UInt128& value) {
    std::stringstream ss;
    ss << "0x" << std::hex << std::uppercase;

    // Get high and low 64-bit parts
    uint64_t high = absl::Uint128High64(value);
    uint64_t low = absl::Uint128Low64(value);

    // Print high part only if non-zero
    if (high != 0) {
        ss << std::setfill('0') << std::setw(16) << high;
    }
    ss << std::setfill('0') << std::setw(16) << low;

    return ss.str();
}

// Convert Int256 to string representation
inline std::string ToString(const Int256& value) {
    // Check if it's just the low part
    if (value.first == 0) {
        return ToString(value.second);
    }

    bool is_negative = value.first < 0;
    Int256 abs_value;

    // Handle negative numbers by negating both parts
    if (is_negative) {
        abs_value.first = -value.first;
        abs_value.second = -value.second-1;
    } else {
        abs_value = value;
    }

    std::string result;
    Int256 num = abs_value;

    // Use the same division algorithm as UInt256 for consistency
    while (num.first != 0 || num.second != 0) {
        Int256 quotient;
        quotient.first = 0;
        quotient.second = 0;
        uint64_t carry = 0;

        // Process from high to low, treating as unsigned for division
        uint64_t* ptr = reinterpret_cast<uint64_t*>(&num);
        uint64_t* qptr = reinterpret_cast<uint64_t*>(&quotient);

        for (int i = 3; i >= 0; --i) {
            uint64_t temp;
            if (i == 3) {
                temp = ptr[i];
            } else {
                temp = (carry << 64) | ptr[i];
            }
            carry = temp % 10;
            qptr[i] = temp / 10;
        }

        result = static_cast<char>('0' + carry) + result;
        num = quotient;
    }

    if (result.empty()) {
        result = "0";
    }
    return is_negative ? "-" + result : result;
}

// Convert UInt256 to string representation
inline std::string ToString(const UInt256& value) {
    std::stringstream ss;
    const auto& [high, low] = value;

    ss << ToString(high);
    if (low > 0) {
        ss << std::setfill('0') << std::setw(38) << ToString(low);
    }
    return ss.str();
}

// Convert Int256 to hexadecimal string representation
inline std::string ToHexString(const Int256& value) {
    std::stringstream ss;
    const auto& [high, low] = value;  // high is Int128, low is UInt128

    ss << "0x" << std::hex << std::uppercase;

    // For Int256, always show full representation
    std::string high_hex = ToHexString(high).substr(2);  // Remove "0x" prefix
    std::string low_hex = ToHexString(low).substr(2);   // Remove "0x" prefix

    // Ensure both parts are properly padded
    ss << std::setfill('0') << std::setw(32) << high_hex;
    ss << std::setfill('0') << std::setw(32) << low_hex;

    return ss.str();
}

// Convert UInt256 to hexadecimal string representation
inline std::string ToHexString(const UInt256& value) {
    std::stringstream ss;
    const auto& [high, low] = value;

    ss << "0x" << std::hex << std::uppercase;
    if (high > 0) {
        ss << ToHexString(high).substr(2);  // Remove "0x" prefix from high part
    }
    ss << std::setfill('0') << std::setw(32) << ToHexString(low).substr(2);  // Remove "0x" prefix from low part

    return ss.str();
}

} // namespace clickhouse
