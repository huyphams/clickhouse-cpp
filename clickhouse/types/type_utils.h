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


// Function to convert bytes to a reversed hex string
std::string ubytesToHexString(const unsigned char* bytes, int length) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    // Iterate in reverse order to reverse byte order
    for (int i = 0; i < length; ++i) {
        oss << std::setw(2) << static_cast<int>(bytes[length - 1 - i]);
    }

    return oss.str();
}

std::string umultiplyLargeNumbers(std::string a, int b) {
    std::string result;
    int carry = 0;

    // Use size_t to avoid implicit conversion warnings
    for (int i = (int)a.length(); i > 0; --i) {
        int product = (a[i - 1] - '0') * b + carry;
        carry = product / 10;
        result.insert(result.begin(), (product % 10) + '0');
    }

    // Add remaining carry
    while (carry > 0) {
        result.insert(result.begin(), (carry % 10) + '0');
        carry /= 10;
    }

    return result;
}

std::string uaddLargeNumbers(std::string a, std::string b) {
    std::string result;
    int carry = 0;

    size_t i = a.length(), j = b.length();

    // Add digits from right to left
    while (i > 0 || j > 0 || carry > 0) {
        int digitA = (i > 0) ? (a[--i] - '0') : 0;
        int digitB = (j > 0) ? (b[--j] - '0') : 0;

        int sum = digitA + digitB + carry;
        carry = sum / 10;
        result.insert(result.begin(), (sum % 10) + '0');
    }

    return result;
}

// Function to convert a large hex string to decimal
std::string uhexToNumeric(const std::string& hexString) {
    std::string decimalValue = "0";

    // Process each hex digit
    for (char hexDigit : hexString) {
        int num;
        if (hexDigit >= '0' && hexDigit <= '9') num = hexDigit - '0';
        else if (hexDigit >= 'a' && hexDigit <= 'f') num = hexDigit - 'a' + 10;
        else if (hexDigit >= 'A' && hexDigit <= 'F') num = hexDigit - 'A' + 10;
        else continue; // Ignore invalid chars

        // Multiply current decimal value by 16 (shift left in base-10)
        decimalValue = umultiplyLargeNumbers(decimalValue, 16);

        // Add current hex digit to decimal value
        decimalValue = uaddLargeNumbers(decimalValue, std::to_string(num));
    }

    return decimalValue;
}

// Convert Int256 to a hex string with sign
inline std::string ToString(const UInt256 &value) {
  return uhexToNumeric(ubytesToHexString(value.bytes, 32));
}

// Function to convert bytes to a reversed hex string
std::string bytesToHexString(const unsigned char* bytes, size_t length) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    // Iterate in reverse order to reverse byte order (little-endian to big-endian)
    for (size_t i = 0; i < length; ++i) {
        oss << std::setw(2) << static_cast<int>(bytes[length - 1 - i]);
    }

    return oss.str();
}

// Function to compute two's complement for negative numbers
std::string twosComplement(const std::string& hexString) {
    std::string complement;
    bool carry = true;  // Start with +1 for two's complement

    // Iterate from the rightmost digit to the left
    for (int i = (int)hexString.length() - 1; i >= 0; --i) {
        char hexDigit = hexString[i];

        // Convert hex digit to integer, invert bits
        int num;
        if (hexDigit >= '0' && hexDigit <= '9') num = 15 - (hexDigit - '0');
        else if (hexDigit >= 'a' && hexDigit <= 'f') num = 15 - (hexDigit - 'a' + 10);
        else if (hexDigit >= 'A' && hexDigit <= 'F') num = 15 - (hexDigit - 'A' + 10);
        else continue;

        // Add 1 for two’s complement
        if (carry) {
            num += 1;
            if (num == 16) {
                num = 0;
                carry = true;
            } else {
                carry = false;
            }
        }

        // Convert back to hex character
        if (num < 10) complement.insert(complement.begin(), '0' + num);
        else complement.insert(complement.begin(), 'a' + (num - 10));
    }

    return complement;
}

// Function to convert a large hex string to decimal (handling signed values)
std::string hexToNumeric(const std::string& hexString) {
    bool isNegative = (hexString[0] >= '8');  // Check MSB for signed value

    std::string decimalValue = "0";
    std::string hexToConvert = isNegative ? twosComplement(hexString) : hexString;

    // Convert hex to decimal
    for (char hexDigit : hexToConvert) {
        int num;
        if (hexDigit >= '0' && hexDigit <= '9') num = hexDigit - '0';
        else if (hexDigit >= 'a' && hexDigit <= 'f') num = hexDigit - 'a' + 10;
        else if (hexDigit >= 'A' && hexDigit <= 'F') num = hexDigit - 'A' + 10;
        else continue;

        // Multiply current decimal value by 16
        decimalValue = std::to_string(std::stoull(decimalValue) * 16);

        // Add current hex digit
        decimalValue = std::to_string(std::stoull(decimalValue) + num);
    }

    return isNegative ? "-" + decimalValue : decimalValue;
}

// Convert Int256 to a hex string with sign
inline std::string ToString(const Int256 &value) {
  return hexToNumeric(bytesToHexString(value.bytes, 32));
}

} // namespace clickhouse
