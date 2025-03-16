#pragma once

#include "types.h"
#include <string>
#include <iomanip>
#include <sstream>
#include "absl/numeric/int128.h"

namespace clickhouse {

inline std::string ToString(const UInt128& value) {
    if (value.first == 0 && value.second == 0) {
        return "0";
    }

    std::string result;
    uint64_t high = value.first;
    uint64_t low = value.second;
    bool started = false;

    // Process high 64 bits first
    if (high != 0) {
        uint64_t temp_high = high;
        while (temp_high > 0) {
            uint64_t remainder = temp_high % 10;
            result = static_cast<char>('0' + remainder) + result;
            temp_high /= 10;
            started = true;
        }
    }

    // Then process low 64 bits
    if (low != 0 || !started) {
        if (started) {
            // If we processed high bits, pad the low bits with zeros
            std::string low_str = std::to_string(low);
            if (high != 0) {
                low_str = std::string(20 - low_str.length(), '0') + low_str;
            }
            result += low_str;
        } else {
            // If no high bits, just convert low bits directly
            result = std::to_string(low);
        }
    }

    return result;
}

inline std::string ToString(const Int128& value) {
    if (value == 0) {
        return "0";
    }

    bool is_negative = value < 0;
    // Convert to uint128 for arithmetic operations
    absl::uint128 abs_val = is_negative ? absl::uint128(-value) : absl::uint128(value);
    std::string result;

    while (abs_val > 0) {
        int digit = static_cast<int>(abs_val % 10);
        result = static_cast<char>('0' + digit) + result;
        abs_val /= 10;
    }

    return is_negative ? "-" + result : result;
}

inline std::string ToString(const UInt256& value) {
    // If high part is 0, just convert the low part
    if (value.first.first == 0 && value.first.second == 0) {
        return ToString(value.second);
    }

    std::string result;
    UInt256 num = value;
    
    while (num.first.first != 0 || num.first.second != 0 || 
           num.second.first != 0 || num.second.second != 0) {
        // Calculate remainder when divided by 10
        UInt256 quotient;
        quotient.first.first = 0;
        quotient.first.second = 0;
        quotient.second.first = 0;
        quotient.second.second = 0;
        uint64_t carry = 0;
        
        // Process from high to low, 64 bits at a time
        uint64_t remainder = num.first.first % 10;
        quotient.first.first = num.first.first / 10;
        
        carry = remainder;
        uint64_t temp = (carry << 64) | num.first.second;
        remainder = temp % 10;
        quotient.first.second = temp / 10;
        
        carry = remainder;
        temp = (carry << 64) | num.second.first;
        remainder = temp % 10;
        quotient.second.first = temp / 10;
        
        carry = remainder;
        temp = (carry << 64) | num.second.second;
        remainder = temp % 10;
        quotient.second.second = temp / 10;

        result = static_cast<char>('0' + remainder) + result;
        num = quotient;
    }

    return result.empty() ? "0" : result;
}

inline std::string ToString(const Int256& value) {
    // Check if it's just the low part
    if (value.first == 0) {
        return ToString(value.second);
    }

    bool is_negative = value.first < 0;
    Int256 abs_value;

    // Handle negative numbers using two's complement
    if (is_negative) {
        if (value.second == 0) {
            abs_value = {-value.first, 0};
        } else {
            // Need to handle borrow
            abs_value.first = -(value.first + 1);
            abs_value.second = -value.second;
        }
    } else {
        abs_value = value;
    }

    std::string result;
    Int256 num = abs_value;
    
    while (num.first != 0 || num.second != 0) {
        Int256 quotient;
        quotient.first = absl::MakeInt128(0, 0);
        quotient.second = absl::MakeInt128(0, 0);
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

// Helper function to parse string to UInt128
inline UInt128 ParseUInt128(const std::string& str) {
    if (str.length() <= 20) {
        return {0, std::stoull(str)};
    }
    size_t split = str.length() - 20;
    return {std::stoull(str.substr(0, split)), 
            std::stoull(str.substr(split))};
}

// Helper function to parse string to Int128
inline Int128 ParseInt128(const std::string& str) {
    bool is_negative = str[0] == '-';
    std::string abs_str = is_negative ? str.substr(1) : str;
    
    Int128 result;
    if (abs_str.length() <= 20) {
        result = Int128(std::stoull(abs_str));
    } else {
        size_t split = abs_str.length() - 20;
        uint64_t high = std::stoull(abs_str.substr(0, split));
        uint64_t low = std::stoull(abs_str.substr(split));
        
        // Combine high and low parts
        result = absl::MakeInt128(high, low);
    }
    
    return is_negative ? -result : result;
}

// Helper function to parse string to UInt256
inline UInt256 ParseUInt256(const std::string& str) {
    if (str.length() <= 40) {
        return {UInt128{0, 0}, ParseUInt128(str)};
    }
    size_t split = str.length() - 40;
    return {ParseUInt128(str.substr(0, split)),
            ParseUInt128(str.substr(split))};
}

// Helper function to parse string to Int256
inline Int256 ParseInt256(const std::string& str) {
    bool is_negative = str[0] == '-';
    std::string abs_str = is_negative ? str.substr(1) : str;
    
    Int256 result;
    if (abs_str.length() <= 40) {
        result = {Int128(0), ParseInt128(abs_str)};
    } else {
        size_t split = abs_str.length() - 40;
        result = {ParseInt128(abs_str.substr(0, split)),
                 ParseInt128(abs_str.substr(split))};
    }
    
    if (is_negative) {
        // Convert to two's complement for negative numbers
        if (result.second == 0) {
            result.first = -result.first;
        } else {
            result.first = -(result.first + 1);
            result.second = -result.second;
        }
    }
    return result;
}

}  // namespace clickhouse 