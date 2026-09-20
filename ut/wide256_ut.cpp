#include <clickhouse/types/type_utils.h>
#include <clickhouse/columns/factory.h>
#include <clickhouse/columns/decimal.h>
#include <clickhouse/columns/nullable.h>
#include <clickhouse/columns/lowcardinality.h>
#include <clickhouse/columns/string.h>
#include <clickhouse/base/input.h>
#include <clickhouse/base/output.h>
#include <gtest/gtest.h>
#include <cstring>

std::string Wide256FormatFromAnotherTranslationUnit(const clickhouse::Int256& value);

namespace {
using namespace clickhouse;
const std::string max_unsigned = "115792089237316195423570985008687907853269984665640564039457584007913129639935";
const std::string max_signed = "57896044618658097711785492504343953926634992332820282019728792003956564819967";
const std::string min_signed = "-57896044618658097711785492504343953926634992332820282019728792003956564819968";

ColumnRef RoundTrip(ColumnRef source) {
    unsigned char wire[4096];
    ArrayOutput output(wire, sizeof(wire));
    source->Save(&output);
    auto result = source->CloneEmpty();
    ArrayInput input(wire, output.Size());
    if (!result->Load(&input, source->Size()) || !input.Exhausted()) {
        throw std::runtime_error("Column wire roundtrip failed");
    }
    return result;
}

TEST(Wide256, ExtremaAndFormatting) {
    for (const auto& text : {std::string("0"), std::string("1"), std::string("-1"), max_signed, min_signed}) {
        auto value = Int256FromString(text);
        EXPECT_EQ(text, ToString(value));
        EXPECT_EQ(text, Wide256FormatFromAnotherTranslationUnit(value));
    }
    EXPECT_EQ("0", ToString(UInt256{}));
    EXPECT_EQ(max_unsigned, ToString(UInt256FromString(max_unsigned)));
    EXPECT_EQ("-170141183460469231731687303715884105728", ToString(std::numeric_limits<Int128>::min()));
    EXPECT_EQ("340282366920938463463374607431768211455", ToString(std::numeric_limits<UInt128>::max()));
}

TEST(Wide256, RejectsMalformedAndOverflow) {
    for (const auto* text : {"", "-", "+1", "1x", "1.0", " 1"}) EXPECT_THROW(Int256FromString(text), ValidationError);
    EXPECT_THROW(UInt256FromString("-1"), ValidationError);
    EXPECT_THROW(UInt256FromString(max_unsigned + "0"), ValidationError);
    EXPECT_THROW(Int256FromString("57896044618658097711785492504343953926634992332820282019728792003956564819968"), ValidationError);
    EXPECT_THROW(Int256FromString("-57896044618658097711785492504343953926634992332820282019728792003956564819969"), ValidationError);
    EXPECT_EQ("0", ToString(Int256FromString("-0")));
}

TEST(Wide256, WireBytesAndContainers) {
    auto source = CreateColumnByType("Int256")->As<ColumnInt256>();
    for (const auto& text : {min_signed, max_signed, std::string("-1"), std::string("256")}) source->Append(Int256FromString(text));
    unsigned char wire[128];
    ArrayOutput output(wire, sizeof(wire));
    source->Save(&output);
    ASSERT_EQ(sizeof(wire), output.Size());
    EXPECT_EQ(0x80, wire[31]);
    EXPECT_EQ(0x7F, wire[63]);
    for (size_t i = 64; i < 96; ++i) EXPECT_EQ(0xFF, wire[i]);
    EXPECT_EQ(0, wire[96]); EXPECT_EQ(1, wire[97]);
    auto loaded = RoundTrip(source)->As<ColumnInt256>();
    for (size_t i = 0; i < source->Size(); ++i) EXPECT_EQ(ToString(source->At(i)), ToString(loaded->GetItem(i).get<Int256>()));
    EXPECT_EQ(max_signed, ToString(source->Slice(1, 1)->As<ColumnInt256>()->At(0)));
    EXPECT_EQ(0u, source->CloneEmpty()->Size());
    auto unsigned_col = CreateColumnByType("UInt256")->As<ColumnUInt256>();
    unsigned_col->Append(UInt256FromString(max_unsigned));
    EXPECT_EQ(max_unsigned, ToString(RoundTrip(unsigned_col)->As<ColumnUInt256>()->At(0)));
}

TEST(Wide256, DecimalFullPrecisionAndScale) {
    auto decimal = CreateColumnByType("Decimal256(10)")->As<ColumnDecimal>();
    const std::string value = std::string(66, '9') + ".1234567890";
    decimal->Append(value);
    decimal->Append("-" + value);
    decimal->Append("0.0000000001");
    EXPECT_EQ(value, decimal->StringAt(0));
    EXPECT_EQ("-" + value, decimal->StringAt(1));
    EXPECT_EQ("0.0000000001", decimal->StringAt(2));
    EXPECT_THROW(decimal->At(0), ValidationError);
    EXPECT_EQ("1", ToString(decimal->At(2)));
    EXPECT_EQ(std::string(66, '9') + "1234567890", ToString(decimal->At256(0)));
    auto loaded = RoundTrip(decimal)->As<ColumnDecimal>();
    for (size_t i = 0; i < decimal->Size(); ++i) EXPECT_EQ(decimal->StringAt(i), loaded->StringAt(i));
    EXPECT_EQ(32u, decimal->GetItem(0).data.size());
    EXPECT_THROW(decimal->Append(std::string(67, '9') + ".0"), ValidationError);
    ColumnDecimal tiny(76, 76); tiny.Append("0." + std::string(75, '0') + "1");
    EXPECT_EQ("0." + std::string(75, '0') + "1", tiny.StringAt(0));
    ColumnDecimal integer(76, 0); integer.Append(std::string(76, '9'));
    EXPECT_EQ(std::string(76, '9'), integer.StringAt(0));
}

TEST(Wide256, Decimal128BoundaryAndValidation) {
    ColumnDecimal decimal(76, 0);
    for (auto value : {std::numeric_limits<Int128>::min(), std::numeric_limits<Int128>::max(), Int128(-1), Int128(0)}) {
        decimal.Append(value);
        EXPECT_EQ(ToString(value), decimal.StringAt(decimal.Size() - 1));
        EXPECT_EQ(ToString(value), ToString(decimal.At(decimal.Size() - 1)));
    }
    EXPECT_THROW(ColumnDecimal(77, 0), ValidationError);
    EXPECT_THROW(ColumnDecimal(0, 0), ValidationError);
    EXPECT_THROW(ColumnDecimal(9, 10), ValidationError);
    ColumnDecimal legacy(18, 3); legacy.Append("-12.345");
    EXPECT_EQ("-12.345", legacy.StringAt(0));
}

TEST(Wide256, LowCardinalityDictionaryRoundtrip) {
    auto source = std::make_shared<ColumnInt256>();
    source->Append(Int256FromString(min_signed)); source->Append(Int256FromString("0")); source->Append(Int256FromString(min_signed));
    auto lc = CreateColumnByType("LowCardinality(Int256)"); lc->Append(source);
    auto loaded = RoundTrip(lc);
    for (size_t i = 0; i < source->Size(); ++i) EXPECT_EQ(ToString(source->At(i)), ToString(loaded->GetItem(i).get<Int256>()));
    auto unsigned_source = std::make_shared<ColumnUInt256>(); unsigned_source->Append(UInt256FromString(max_unsigned));
    auto unsigned_lc = CreateColumnByType("LowCardinality(UInt256)"); unsigned_lc->Append(unsigned_source);
    EXPECT_EQ(max_unsigned, ToString(RoundTrip(unsigned_lc)->GetItem(0).get<UInt256>()));
}

TEST(Wide256, WrappedNullableLowCardinality) {
    auto values = std::make_shared<ColumnNullableT<ColumnString>>();
    values->Append(std::optional<std::string_view>("alpha")); values->Append(std::nullopt); values->Append(std::optional<std::string_view>(""));
    auto source = std::make_shared<ColumnLowCardinalityT<ColumnNullableT<ColumnString>>>(values);
    unsigned char wire[4096]; ArrayOutput output(wire, sizeof(wire)); source->Save(&output);
    CreateColumnByTypeSettings settings; settings.low_cardinality_as_wrapped_column = true;
    auto wrapped = CreateColumnByType("LowCardinality(Nullable(String))", settings);
    ArrayInput input(wire, output.Size()); ASSERT_TRUE(wrapped->Load(&input, 3)); EXPECT_TRUE(input.Exhausted());
    auto nullable = wrapped->As<ColumnNullable>(); ASSERT_NE(nullptr, nullable);
    EXPECT_FALSE(nullable->IsNull(0)); EXPECT_TRUE(nullable->IsNull(1)); EXPECT_FALSE(nullable->IsNull(2));
    EXPECT_EQ("alpha", nullable->Nested()->As<ColumnString>()->At(0));
    EXPECT_EQ("", nullable->Nested()->As<ColumnString>()->At(2));
}
} // namespace
