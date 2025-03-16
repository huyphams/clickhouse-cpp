#pragma once

#include <cstdint>
#include <utility>

namespace clickhouse {

using _UInt128 = std::pair<uint64_t, uint64_t>;

using UUID = _UInt128;

}
