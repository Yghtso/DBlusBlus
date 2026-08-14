#include "common/encoding.h"
#include "storage/heap_page.h"
#include "storage/page.h"
#include "storage/tuple_codec.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace dblusblus {
namespace {

template <typename Value>
[[nodiscard]] const Value* RequireOptional(const std::optional<Value>& optional,
                                           std::string_view description) {
    if (!optional.has_value()) {
        ADD_FAILURE() << description << " unexpectedly missing";
        return nullptr;
    }
    return std::addressof(*optional);
}

void ExpectScalarRoundTrip(PhysicalType type,
                           const FixedTupleValue& expected_value,
                           std::span<const std::byte> expected_bytes) {
    std::array<std::byte, TIMESTAMP_PHYSICAL_SIZE> encoded{};
    ASSERT_EQ(EncodeFixedScalar(encoded, type, expected_value), FixedScalarCodecError::NONE);
    ASSERT_LE(expected_bytes.size(), encoded.size());
    EXPECT_TRUE(
        std::ranges::equal(expected_bytes, std::span{encoded}.first(expected_bytes.size())));

    const auto decoded = DecodeFixedScalar(type, expected_bytes);
    if (!decoded.value.has_value()) {
        ADD_FAILURE() << "fixed scalar unexpectedly failed to decode";
        return;
    }
    EXPECT_EQ(decoded.error, FixedScalarCodecError::NONE);
    EXPECT_EQ(*decoded.value, expected_value);
}

void ExpectTupleValue(const TuplePhysicalLayout& layout,
                      std::span<const std::byte> tuple,
                      std::size_t column_index,
                      const FixedTupleValue& expected) {
    const auto decoded = DecodeFixedTupleValue(layout, tuple, column_index);
    if (!decoded.value.has_value()) {
        ADD_FAILURE() << "fixed tuple column " << column_index << " unexpectedly failed to decode";
        return;
    }
    EXPECT_EQ(decoded.error, FixedTupleCodecError::NONE);
    EXPECT_EQ(*decoded.value, expected);
}

TEST(FixedScalarCodecTest, EncodesBooleanAsExactlyZeroOrOneAndRejectsOtherBytes) {
    constexpr std::array false_bytes{std::byte{0x00}};
    constexpr std::array true_bytes{std::byte{0x01}};
    ExpectScalarRoundTrip(PhysicalType::BOOLEAN, FixedTupleValue{false}, false_bytes);
    ExpectScalarRoundTrip(PhysicalType::BOOLEAN, FixedTupleValue{true}, true_bytes);

    constexpr std::array malformed{std::byte{0x02}};
    const auto decoded = DecodeFixedScalar(PhysicalType::BOOLEAN, malformed);
    EXPECT_FALSE(decoded.value.has_value());
    EXPECT_EQ(decoded.error, FixedScalarCodecError::INVALID_BOOLEAN);
}

TEST(FixedScalarCodecTest, EncodesInt32TwosComplementLittleEndianExactly) {
    struct Case {
        std::int32_t value;
        std::array<std::byte, INT32_PHYSICAL_SIZE> bytes;
    };
    constexpr std::array cases{
        Case{.value = 0,
             .bytes = {std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}}},
        Case{.value = 1,
             .bytes = {std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}}},
        Case{.value = -1,
             .bytes = {std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}}},
        Case{
            .value = std::numeric_limits<std::int32_t>::min(),
            .bytes = {std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80}},
        },
        Case{
            .value = std::numeric_limits<std::int32_t>::max(),
            .bytes = {std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x7F}},
        },
    };

    for (const auto& test_case : cases) {
        ExpectScalarRoundTrip(
            PhysicalType::INT32, FixedTupleValue{test_case.value}, test_case.bytes);
    }
}

TEST(FixedScalarCodecTest, EncodesInt64TwosComplementLittleEndianExactly) {
    struct Case {
        std::int64_t value;
        std::array<std::byte, INT64_PHYSICAL_SIZE> bytes;
    };
    constexpr std::array cases{
        Case{
            .value = 0,
            .bytes = {std::byte{0x00},
                      std::byte{0x00},
                      std::byte{0x00},
                      std::byte{0x00},
                      std::byte{0x00},
                      std::byte{0x00},
                      std::byte{0x00},
                      std::byte{0x00}},
        },
        Case{
            .value = -1,
            .bytes = {std::byte{0xFF},
                      std::byte{0xFF},
                      std::byte{0xFF},
                      std::byte{0xFF},
                      std::byte{0xFF},
                      std::byte{0xFF},
                      std::byte{0xFF},
                      std::byte{0xFF}},
        },
        Case{
            .value = std::numeric_limits<std::int64_t>::min(),
            .bytes = {std::byte{0x00},
                      std::byte{0x00},
                      std::byte{0x00},
                      std::byte{0x00},
                      std::byte{0x00},
                      std::byte{0x00},
                      std::byte{0x00},
                      std::byte{0x80}},
        },
        Case{
            .value = std::numeric_limits<std::int64_t>::max(),
            .bytes = {std::byte{0xFF},
                      std::byte{0xFF},
                      std::byte{0xFF},
                      std::byte{0xFF},
                      std::byte{0xFF},
                      std::byte{0xFF},
                      std::byte{0xFF},
                      std::byte{0x7F}},
        },
    };

    for (const auto& test_case : cases) {
        ExpectScalarRoundTrip(
            PhysicalType::INT64, FixedTupleValue{test_case.value}, test_case.bytes);
    }
}

TEST(FixedScalarCodecTest, PreservesEveryFloat64PayloadBit) {
    constexpr std::array bits{
        std::uint64_t{0x0000000000000000ULL}, // +0
        std::uint64_t{0x8000000000000000ULL}, // -0
        std::uint64_t{0x3FF0000000000000ULL}, // 1.0
        std::uint64_t{0xC004000000000000ULL}, // -2.5
        std::uint64_t{0x7FF0000000000000ULL}, // +infinity
        std::uint64_t{0xFFF0000000000000ULL}, // -infinity
        std::uint64_t{0x7FF8123456789ABCULL}, // quiet NaN with payload
    };

    for (const auto expected_bits : bits) {
        std::array<std::byte, FLOAT64_PHYSICAL_SIZE> expected_bytes{};
        ASSERT_TRUE(EncodeLittleEndian(expected_bytes, expected_bits));
        const auto value = Float64PhysicalValue::FromDouble(std::bit_cast<double>(expected_bits));
        ASSERT_EQ(value.bits, expected_bits);
        ExpectScalarRoundTrip(PhysicalType::FLOAT64, FixedTupleValue{value}, expected_bytes);

        const auto decoded = DecodeFixedScalar(PhysicalType::FLOAT64, expected_bytes);
        const auto* decoded_value = RequireOptional(decoded.value, "decoded FLOAT64");
        if (decoded_value == nullptr) {
            return;
        }
        const auto decoded_float = std::get<Float64PhysicalValue>(*decoded_value);
        EXPECT_EQ(decoded_float.bits, expected_bits);
        EXPECT_EQ(std::bit_cast<std::uint64_t>(decoded_float.ToDouble()), expected_bits);
    }
}

TEST(FixedScalarCodecTest, TreatsDateAndTimestampAsRawSignedPhysicalScalars) {
    constexpr std::array date_positive{
        std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01}};
    constexpr std::array date_negative{
        std::byte{0xFC}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}};
    ExpectScalarRoundTrip(
        PhysicalType::DATE, FixedTupleValue{DatePhysicalValue{.value = 0x01020304}}, date_positive);
    ExpectScalarRoundTrip(
        PhysicalType::DATE, FixedTupleValue{DatePhysicalValue{.value = -4}}, date_negative);

    constexpr std::array timestamp_positive{std::byte{0x08},
                                            std::byte{0x07},
                                            std::byte{0x06},
                                            std::byte{0x05},
                                            std::byte{0x04},
                                            std::byte{0x03},
                                            std::byte{0x02},
                                            std::byte{0x01}};
    constexpr std::array timestamp_negative{std::byte{0xF8},
                                            std::byte{0xFF},
                                            std::byte{0xFF},
                                            std::byte{0xFF},
                                            std::byte{0xFF},
                                            std::byte{0xFF},
                                            std::byte{0xFF},
                                            std::byte{0xFF}};
    ExpectScalarRoundTrip(PhysicalType::TIMESTAMP,
                          FixedTupleValue{TimestampPhysicalValue{.value = 0x0102030405060708LL}},
                          timestamp_positive);
    ExpectScalarRoundTrip(PhysicalType::TIMESTAMP,
                          FixedTupleValue{TimestampPhysicalValue{.value = -8}},
                          timestamp_negative);
}

TEST(FixedScalarCodecTest, RejectsMismatchesVarlenInvalidTypesAndShortBuffers) {
    std::array<std::byte, INT64_PHYSICAL_SIZE> destination{};
    EXPECT_EQ(EncodeFixedScalar(destination, PhysicalType::INT32, FixedTupleValue{std::int64_t{1}}),
              FixedScalarCodecError::TYPE_MISMATCH);
    EXPECT_EQ(
        EncodeFixedScalar(destination, PhysicalType::INT32, FixedTupleValue{std::monostate{}}),
        FixedScalarCodecError::TYPE_MISMATCH);
    EXPECT_EQ(EncodeFixedScalar(destination, PhysicalType::VARCHAR, FixedTupleValue{false}),
              FixedScalarCodecError::UNSUPPORTED_VARLEN_TYPE);
    EXPECT_EQ(
        EncodeFixedScalar(destination, static_cast<PhysicalType>(0xFFU), FixedTupleValue{false}),
        FixedScalarCodecError::INVALID_PHYSICAL_TYPE);
    EXPECT_EQ(EncodeFixedScalar(std::span{destination}.first(3),
                                PhysicalType::INT32,
                                FixedTupleValue{std::int32_t{1}}),
              FixedScalarCodecError::DESTINATION_TOO_SMALL);

    EXPECT_EQ(DecodeFixedScalar(PhysicalType::INT64, std::span{destination}.first(7)).error,
              FixedScalarCodecError::SOURCE_TOO_SMALL);
    EXPECT_EQ(DecodeFixedScalar(PhysicalType::VARCHAR, destination).error,
              FixedScalarCodecError::UNSUPPORTED_VARLEN_TYPE);
    EXPECT_EQ(DecodeFixedScalar(static_cast<PhysicalType>(0xFFU), destination).error,
              FixedScalarCodecError::INVALID_PHYSICAL_TYPE);
}

TEST(FixedTupleCodecTest, EncodesAndDecodesExactMixedFixedTuple) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::BOOLEAN, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = true},
        PhysicalColumnSpec{.type = PhysicalType::INT64, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::FLOAT64, .nullable = true},
        PhysicalColumnSpec{.type = PhysicalType::DATE, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::TIMESTAMP, .nullable = true},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{9});
    const auto* layout_pointer = RequireOptional(built.layout, "mixed fixed layout");
    if (layout_pointer == nullptr) {
        return;
    }
    const auto& layout = *layout_pointer;
    const auto float_value = Float64PhysicalValue::FromDouble(-2.5);
    const std::array values{
        FixedTupleValue{true},
        FixedTupleValue{std::int32_t{0x01020304}},
        FixedTupleValue{std::int64_t{-1}},
        FixedTupleValue{float_value},
        FixedTupleValue{DatePhysicalValue{.value = -2}},
        FixedTupleValue{TimestampPhysicalValue{.value = 0x0102030405060708LL}},
    };
    const TupleVersionMetadata metadata{
        .xmin = TxnId{17},
        .xmax = TxnId{23},
        .cmin = CommandId{3},
        .cmax = CommandId{5},
        .prev_page_no = PageNo{31},
        .prev_slot = SlotId{7},
    };

    const auto encoded = EncodeFixedTuple(layout, metadata, values);
    const auto* tuple_pointer = RequireOptional(encoded.tuple, "mixed encoded tuple");
    if (tuple_pointer == nullptr) {
        return;
    }
    const auto& tuple = *tuple_pointer;
    ASSERT_EQ(tuple.size(), 82U);
    EXPECT_EQ(layout.MinimumTupleSize(), 82U);
    ASSERT_EQ(layout.FixedAreaOffset(), 49U);
    ASSERT_EQ(layout.Columns()[0].fixed_offset, 49U);
    ASSERT_EQ(layout.Columns()[1].fixed_offset, 50U);
    ASSERT_EQ(layout.Columns()[2].fixed_offset, 54U);
    ASSERT_EQ(layout.Columns()[3].fixed_offset, 62U);
    ASSERT_EQ(layout.Columns()[4].fixed_offset, 70U);
    ASSERT_EQ(layout.Columns()[5].fixed_offset, 74U);

    const auto validation = ValidateFixedTuple(layout, tuple);
    const auto* header = RequireOptional(validation.header, "validated mixed tuple header");
    if (header == nullptr) {
        return;
    }
    EXPECT_EQ(*header,
              (TupleHeader{
                  .xmin = metadata.xmin,
                  .xmax = metadata.xmax,
                  .cmin = metadata.cmin,
                  .cmax = metadata.cmax,
                  .prev_page_no = metadata.prev_page_no,
                  .prev_slot = metadata.prev_slot,
                  .tuple_flags = 0,
                  .header_bytes = TUPLE_HEADER_ENCODED_SIZE,
                  .null_bitmap_bytes = 1,
                  .schema_version = layout.SchemaVersion(),
                  .reserved = 0,
              }));
    EXPECT_EQ(tuple[TUPLE_HEADER_ENCODED_SIZE], std::byte{0x00});
    EXPECT_EQ(tuple[49], std::byte{0x01});
    EXPECT_TRUE(std::ranges::equal(
        std::span{tuple}.subspan(50, 4),
        std::array{std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01}}));
    EXPECT_TRUE(std::ranges::equal(std::span{tuple}.subspan(54, 8),
                                   std::array{std::byte{0xFF},
                                              std::byte{0xFF},
                                              std::byte{0xFF},
                                              std::byte{0xFF},
                                              std::byte{0xFF},
                                              std::byte{0xFF},
                                              std::byte{0xFF},
                                              std::byte{0xFF}}));
    std::array<std::byte, 8> expected_float{};
    ASSERT_TRUE(EncodeLittleEndian(expected_float, float_value.bits));
    EXPECT_TRUE(std::ranges::equal(std::span{tuple}.subspan(62, 8), expected_float));
    EXPECT_TRUE(std::ranges::equal(
        std::span{tuple}.subspan(70, 4),
        std::array{std::byte{0xFE}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}}));
    EXPECT_TRUE(std::ranges::equal(std::span{tuple}.subspan(74, 8),
                                   std::array{std::byte{0x08},
                                              std::byte{0x07},
                                              std::byte{0x06},
                                              std::byte{0x05},
                                              std::byte{0x04},
                                              std::byte{0x03},
                                              std::byte{0x02},
                                              std::byte{0x01}}));

    for (std::size_t index = 0; index < values.size(); ++index) {
        ExpectTupleValue(layout, tuple, index, values[index]);
    }
}

TEST(FixedTupleCodecTest, DerivesNullBitmapFlagAndZeroesNullFixedBytesAcrossBoundary) {
    std::array<PhysicalColumnSpec, 10> columns{};
    columns.fill(PhysicalColumnSpec{.type = PhysicalType::BOOLEAN, .nullable = true});
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{1});
    const auto* layout_pointer = RequireOptional(built.layout, "bitmap boundary layout");
    if (layout_pointer == nullptr) {
        return;
    }
    const auto& layout = *layout_pointer;

    std::array<FixedTupleValue, 10> values{};
    values.fill(FixedTupleValue{true});
    values[0] = FixedTupleValue{std::monostate{}};
    values[8] = FixedTupleValue{std::monostate{}};
    const auto encoded = EncodeFixedTuple(layout, TupleVersionMetadata{}, values);
    const auto* tuple_pointer = RequireOptional(encoded.tuple, "bitmap boundary tuple");
    if (tuple_pointer == nullptr) {
        return;
    }
    const auto& tuple = *tuple_pointer;
    ASSERT_EQ(tuple.size(), 60U);
    EXPECT_EQ(tuple[48], std::byte{0x01});
    EXPECT_EQ(tuple[49], std::byte{0x01});
    EXPECT_EQ(std::to_integer<std::uint8_t>(tuple[49]) & 0xFCU, 0U);
    EXPECT_EQ(tuple[layout.Columns()[0].fixed_offset], std::byte{0x00});
    EXPECT_EQ(tuple[layout.Columns()[8].fixed_offset], std::byte{0x00});
    EXPECT_EQ(tuple[layout.Columns()[1].fixed_offset], std::byte{0x01});

    const auto header = DecodeTupleHeader(tuple);
    const auto* decoded_header = RequireOptional(header.header, "bitmap boundary tuple header");
    if (decoded_header == nullptr) {
        return;
    }
    EXPECT_EQ(decoded_header->tuple_flags, TUPLE_FLAG_HAS_NULLS);
    ExpectTupleValue(layout, tuple, 0, FixedTupleValue{std::monostate{}});
    ExpectTupleValue(layout, tuple, 8, FixedTupleValue{std::monostate{}});
    ExpectTupleValue(layout, tuple, 9, FixedTupleValue{true});
}

TEST(FixedTupleCodecTest, ClearsHasNullsWhenAllNullableColumnsArePresent) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::BOOLEAN, .nullable = true},
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = true},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{2});
    const auto* layout = RequireOptional(built.layout, "all-present layout");
    if (layout == nullptr) {
        return;
    }
    const std::array values{FixedTupleValue{false}, FixedTupleValue{std::int32_t{0}}};

    const auto encoded = EncodeFixedTuple(*layout, TupleVersionMetadata{}, values);
    const auto* tuple = RequireOptional(encoded.tuple, "all-present tuple");
    if (tuple == nullptr) {
        return;
    }
    const auto header = DecodeTupleHeader(*tuple);
    const auto* decoded_header = RequireOptional(header.header, "all-present tuple header");
    if (decoded_header == nullptr) {
        return;
    }
    EXPECT_EQ(decoded_header->tuple_flags, 0U);
    EXPECT_EQ((*tuple)[48], std::byte{0x00});
}

TEST(FixedTupleCodecTest, RejectsNullForNotNullColumnWithoutTuple) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = false},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{1});
    const auto* layout = RequireOptional(built.layout, "NOT NULL layout");
    if (layout == nullptr) {
        return;
    }
    const std::array values{FixedTupleValue{std::monostate{}}};

    const auto encoded = EncodeFixedTuple(*layout, TupleVersionMetadata{}, values);
    EXPECT_FALSE(encoded.tuple.has_value());
    EXPECT_EQ(encoded.error, FixedTupleCodecError::NULL_NOT_ALLOWED);
    EXPECT_EQ(encoded.column_index, 0U);
}

TEST(FixedTupleCodecTest, RejectsEveryFixedTypeMismatchWithoutTuple) {
    struct Case {
        PhysicalType expected_type;
        FixedTupleValue wrong_value;
    };
    const std::array cases{
        Case{.expected_type = PhysicalType::BOOLEAN, .wrong_value = std::int32_t{1}},
        Case{.expected_type = PhysicalType::INT32, .wrong_value = std::int64_t{1}},
        Case{.expected_type = PhysicalType::INT64,
             .wrong_value = TimestampPhysicalValue{.value = 1}},
        Case{.expected_type = PhysicalType::FLOAT64, .wrong_value = std::int64_t{1}},
        Case{.expected_type = PhysicalType::DATE, .wrong_value = std::int32_t{1}},
        Case{.expected_type = PhysicalType::TIMESTAMP, .wrong_value = std::int64_t{1}},
    };

    for (const auto& test_case : cases) {
        const std::array columns{
            PhysicalColumnSpec{.type = test_case.expected_type, .nullable = false}};
        const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{1});
        const auto* layout = RequireOptional(built.layout, "type-mismatch layout");
        if (layout == nullptr) {
            return;
        }
        const std::array values{test_case.wrong_value};
        const auto encoded = EncodeFixedTuple(*layout, TupleVersionMetadata{}, values);
        EXPECT_FALSE(encoded.tuple.has_value());
        EXPECT_EQ(encoded.error, FixedTupleCodecError::TYPE_MISMATCH);
        EXPECT_EQ(encoded.column_index, 0U);
    }
}

TEST(FixedTupleCodecTest, RejectsColumnCountVarlenAndInvalidMetadataWithoutTuple) {
    constexpr std::array fixed_columns{
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = false},
    };
    const auto fixed_built = BuildTuplePhysicalLayout(fixed_columns, SchemaVer{1});
    const auto* fixed_layout = RequireOptional(fixed_built.layout, "fixed count layout");
    if (fixed_layout == nullptr) {
        return;
    }
    EXPECT_EQ(EncodeFixedTuple(*fixed_layout, TupleVersionMetadata{}, {}).error,
              FixedTupleCodecError::COLUMN_COUNT_MISMATCH);

    constexpr std::array varchar_columns{
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = true},
    };
    const auto varchar_built = BuildTuplePhysicalLayout(varchar_columns, SchemaVer{1});
    const auto* varchar_layout = RequireOptional(varchar_built.layout, "VARCHAR layout");
    if (varchar_layout == nullptr) {
        return;
    }
    const std::array null_value{FixedTupleValue{std::monostate{}}};
    EXPECT_EQ(EncodeFixedTuple(*varchar_layout, TupleVersionMetadata{}, null_value).error,
              FixedTupleCodecError::UNSUPPORTED_VARLEN_TYPE);
    const std::vector<std::byte> unencoded_varchar_tuple(varchar_layout->MinimumTupleSize(),
                                                         std::byte{0});
    EXPECT_EQ(ValidateFixedTuple(*varchar_layout, unencoded_varchar_tuple).error,
              FixedTupleCodecError::UNSUPPORTED_VARLEN_TYPE);

    const TupleVersionMetadata malformed_previous{
        .prev_page_no = INVALID_PAGE_NO,
        .prev_slot = SlotId{3},
    };
    const std::array fixed_value{FixedTupleValue{std::int32_t{1}}};
    const auto malformed = EncodeFixedTuple(*fixed_layout, malformed_previous, fixed_value);
    EXPECT_FALSE(malformed.tuple.has_value());
    EXPECT_EQ(malformed.error, FixedTupleCodecError::INVALID_HEADER_METADATA);
}

TEST(FixedTupleCodecTest, SupportsZeroColumnSchemaAndLayoutOwnedSchemaVersion) {
    const auto built =
        BuildTuplePhysicalLayout(std::span<const PhysicalColumnSpec>{}, SchemaVer{41});
    const auto* layout_pointer = RequireOptional(built.layout, "zero-column layout");
    if (layout_pointer == nullptr) {
        return;
    }
    const auto& layout = *layout_pointer;
    const TupleVersionMetadata metadata{.xmin = TxnId{11}, .cmin = CommandId{2}};

    const auto encoded = EncodeFixedTuple(layout, metadata, {});
    const auto* tuple = RequireOptional(encoded.tuple, "zero-column tuple");
    if (tuple == nullptr) {
        return;
    }
    EXPECT_EQ(tuple->size(), TUPLE_HEADER_ENCODED_SIZE);
    const auto validation = ValidateFixedTuple(layout, *tuple);
    const auto* header = RequireOptional(validation.header, "zero-column tuple header");
    if (header == nullptr) {
        return;
    }
    EXPECT_EQ(header->schema_version, SchemaVer{41});
    EXPECT_EQ(header->header_bytes, TUPLE_HEADER_ENCODED_SIZE);
    EXPECT_EQ(header->null_bitmap_bytes, 0U);
    EXPECT_EQ(header->tuple_flags, 0U);
    EXPECT_EQ(header->reserved, 0U);
    EXPECT_EQ(DecodeFixedTupleValue(layout, *tuple, 0).error,
              FixedTupleCodecError::COLUMN_OUT_OF_RANGE);

    const auto other_layout =
        BuildTuplePhysicalLayout(std::span<const PhysicalColumnSpec>{}, SchemaVer{42});
    const auto* other_layout_pointer = RequireOptional(other_layout.layout, "other schema layout");
    if (other_layout_pointer == nullptr) {
        return;
    }
    EXPECT_EQ(ValidateFixedTuple(*other_layout_pointer, *tuple).error,
              FixedTupleCodecError::SCHEMA_VERSION_MISMATCH);
}

TEST(FixedTupleCodecTest, StrictlyRejectsFlagBitmapAndUnusedBitNoncanonicalTuples) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::BOOLEAN, .nullable = true},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{1});
    const auto* layout_pointer = RequireOptional(built.layout, "canonical bitmap layout");
    if (layout_pointer == nullptr) {
        return;
    }
    const auto& layout = *layout_pointer;

    const std::array null_value{FixedTupleValue{std::monostate{}}};
    const auto null_encoded = EncodeFixedTuple(layout, TupleVersionMetadata{}, null_value);
    const auto* null_tuple = RequireOptional(null_encoded.tuple, "NULL tuple");
    if (null_tuple == nullptr) {
        return;
    }
    auto null_without_flag = *null_tuple;
    ASSERT_TRUE(EncodeLittleEndian(
        std::span{null_without_flag}.subspan(TUPLE_HEADER_FLAGS_OFFSET, sizeof(TupleFlags)),
        TupleFlags{0}));
    EXPECT_EQ(ValidateFixedTuple(layout, null_without_flag).error,
              FixedTupleCodecError::FLAG_BITMAP_MISMATCH);

    const std::array present_value{FixedTupleValue{true}};
    const auto present_encoded = EncodeFixedTuple(layout, TupleVersionMetadata{}, present_value);
    const auto* present_tuple = RequireOptional(present_encoded.tuple, "present tuple");
    if (present_tuple == nullptr) {
        return;
    }
    auto present_with_flag = *present_tuple;
    ASSERT_TRUE(EncodeLittleEndian(
        std::span{present_with_flag}.subspan(TUPLE_HEADER_FLAGS_OFFSET, sizeof(TupleFlags)),
        TUPLE_FLAG_HAS_NULLS));
    EXPECT_EQ(ValidateFixedTuple(layout, present_with_flag).error,
              FixedTupleCodecError::FLAG_BITMAP_MISMATCH);

    auto unused_bit = *present_tuple;
    unused_bit[TUPLE_HEADER_ENCODED_SIZE] = std::byte{0x80};
    EXPECT_EQ(ValidateFixedTuple(layout, unused_bit).error, FixedTupleCodecError::MALFORMED_TUPLE);

    auto unexpected_varlen = *present_tuple;
    ASSERT_TRUE(EncodeLittleEndian(
        std::span{unexpected_varlen}.subspan(TUPLE_HEADER_FLAGS_OFFSET, sizeof(TupleFlags)),
        TUPLE_FLAG_HAS_VARLEN));
    EXPECT_EQ(ValidateFixedTuple(layout, unexpected_varlen).error,
              FixedTupleCodecError::MALFORMED_TUPLE);
}

TEST(FixedTupleCodecTest, RejectsHeaderLayoutCorruptionAndTruncation) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = false},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{7});
    const auto* layout_pointer = RequireOptional(built.layout, "corruption-test layout");
    if (layout_pointer == nullptr) {
        return;
    }
    const auto& layout = *layout_pointer;
    const std::array values{FixedTupleValue{std::int32_t{1}}};
    const auto encoded = EncodeFixedTuple(layout, TupleVersionMetadata{}, values);
    const auto* tuple = RequireOptional(encoded.tuple, "corruption-test tuple");
    if (tuple == nullptr) {
        return;
    }
    const auto& valid = *tuple;

    auto wrong_header_size = valid;
    ASSERT_TRUE(EncodeLittleEndian(std::span{wrong_header_size}.subspan(
                                       TUPLE_HEADER_HEADER_BYTES_OFFSET, sizeof(std::uint16_t)),
                                   std::uint16_t{47}));
    const auto header_size_result = ValidateFixedTuple(layout, wrong_header_size);
    EXPECT_EQ(header_size_result.error, FixedTupleCodecError::MALFORMED_TUPLE);
    EXPECT_EQ(header_size_result.header_error, TupleHeaderDecodeError::INVALID_HEADER_SIZE);

    auto wrong_bitmap_size = valid;
    ASSERT_TRUE(
        EncodeLittleEndian(std::span{wrong_bitmap_size}.subspan(
                               TUPLE_HEADER_NULL_BITMAP_BYTES_OFFSET, sizeof(std::uint16_t)),
                           std::uint16_t{2}));
    EXPECT_EQ(ValidateFixedTuple(layout, wrong_bitmap_size).error,
              FixedTupleCodecError::MALFORMED_TUPLE);

    auto wrong_schema_version = valid;
    ASSERT_TRUE(EncodeLittleEndian(std::span{wrong_schema_version}.subspan(
                                       TUPLE_HEADER_SCHEMA_VERSION_OFFSET, sizeof(SchemaVer)),
                                   SchemaVer{8}));
    EXPECT_EQ(ValidateFixedTuple(layout, wrong_schema_version).error,
              FixedTupleCodecError::SCHEMA_VERSION_MISMATCH);

    auto unknown_flags = valid;
    ASSERT_TRUE(EncodeLittleEndian(
        std::span{unknown_flags}.subspan(TUPLE_HEADER_FLAGS_OFFSET, sizeof(TupleFlags)),
        TupleFlags{0x0004}));
    EXPECT_EQ(ValidateFixedTuple(layout, unknown_flags).header_error,
              TupleHeaderDecodeError::INVALID_FLAGS);

    auto nonzero_reserved = valid;
    ASSERT_TRUE(EncodeLittleEndian(
        std::span{nonzero_reserved}.subspan(TUPLE_HEADER_RESERVED_OFFSET, sizeof(std::uint32_t)),
        std::uint32_t{1}));
    EXPECT_EQ(ValidateFixedTuple(layout, nonzero_reserved).header_error,
              TupleHeaderDecodeError::NONZERO_RESERVED);

    auto malformed_previous = valid;
    ASSERT_TRUE(EncodeLittleEndian(
        std::span{malformed_previous}.subspan(TUPLE_HEADER_PREV_SLOT_OFFSET, sizeof(SlotId)),
        SlotId{3}));
    EXPECT_EQ(ValidateFixedTuple(layout, malformed_previous).header_error,
              TupleHeaderDecodeError::INVALID_PREVIOUS_VERSION_POINTER);

    EXPECT_EQ(ValidateFixedTuple(layout, std::span{valid}.first(valid.size() - 1U)).error,
              FixedTupleCodecError::MALFORMED_TUPLE);
}

TEST(FixedTupleCodecTest, RejectsMalformedBooleanInOtherwiseValidTuple) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::BOOLEAN, .nullable = false},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{1});
    const auto* layout_pointer = RequireOptional(built.layout, "BOOLEAN corruption layout");
    if (layout_pointer == nullptr) {
        return;
    }
    const auto& layout = *layout_pointer;
    const std::array values{FixedTupleValue{false}};
    const auto encoded = EncodeFixedTuple(layout, TupleVersionMetadata{}, values);
    const auto* tuple = RequireOptional(encoded.tuple, "BOOLEAN corruption tuple");
    if (tuple == nullptr) {
        return;
    }
    auto malformed = *tuple;
    malformed[layout.Columns()[0].fixed_offset] = std::byte{0x02};

    EXPECT_TRUE(ValidateFixedTuple(layout, malformed));
    EXPECT_EQ(DecodeFixedTupleValue(layout, malformed, 0).error,
              FixedTupleCodecError::INVALID_BOOLEAN);
}

TEST(FixedTupleCodecTest, ComposesWithHeapPageOpaqueInsertionAndRetrieval) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::BOOLEAN, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = true},
        PhysicalColumnSpec{.type = PhysicalType::TIMESTAMP, .nullable = false},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{3});
    const auto* layout_pointer = RequireOptional(built.layout, "HeapPage composition layout");
    if (layout_pointer == nullptr) {
        return;
    }
    const auto& layout = *layout_pointer;
    const std::array values{
        FixedTupleValue{true},
        FixedTupleValue{std::monostate{}},
        FixedTupleValue{TimestampPhysicalValue{.value = -17}},
    };
    const auto encoded = EncodeFixedTuple(
        layout, TupleVersionMetadata{.xmin = TxnId{29}, .cmin = CommandId{4}}, values);
    const auto* tuple = RequireOptional(encoded.tuple, "HeapPage composition tuple");
    if (tuple == nullptr) {
        return;
    }

    const PageId page_id{.file_id = FileId{2}, .page_no = PageNo{5}};
    Page page{page_id};
    HeapPage heap_page{page};
    ASSERT_TRUE(heap_page.Initialize());
    const auto inserted = heap_page.Insert(*tuple);
    const auto* rid = RequireOptional(inserted.rid, "inserted tuple RID");
    if (rid == nullptr) {
        return;
    }
    const auto stored = heap_page.TupleBytes(rid->slot);
    const auto* stored_tuple = RequireOptional(stored, "stored tuple bytes");
    if (stored_tuple == nullptr) {
        return;
    }
    EXPECT_TRUE(std::ranges::equal(*stored_tuple, *tuple));

    ExpectTupleValue(layout, *stored_tuple, 0, values[0]);
    ExpectTupleValue(layout, *stored_tuple, 1, values[1]);
    ExpectTupleValue(layout, *stored_tuple, 2, values[2]);
}

} // namespace
} // namespace dblusblus
