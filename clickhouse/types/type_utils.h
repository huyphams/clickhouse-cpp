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

} // namespace clickhouse
