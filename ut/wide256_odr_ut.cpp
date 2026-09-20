#include <clickhouse/types/type_utils.h>

// Link with a second translation unit that includes every inline formatting helper.
std::string Wide256FormatFromAnotherTranslationUnit(const clickhouse::Int256& value) {
    return clickhouse::ToString(value);
}
