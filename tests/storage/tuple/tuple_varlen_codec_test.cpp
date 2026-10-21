#include "common/encoding.h"
#include "storage/disk/disk_manager.h"
#include "storage/file/file_superblock.h"
#include "storage/file/page_file.h"
#include "storage/heap/heap_page.h"
#include "storage/page/page.h"
#include "storage/tuple/tuple_codec.h"
#include "support/require.h"
#include "support/temporary_directory.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <variant>
#include <vector>

namespace dblusblus::storage {
namespace {

using test::RequireValue;

using test::TemporaryDirectory;

[[nodiscard]] VarcharValue Varchar(std::string_view value) noexcept {
    return VarcharValue{
        .bytes = std::as_bytes(std::span<const char>{value.data(), value.size()}),
    };
}

[[nodiscard]] std::optional<VarcharDescriptor> DescriptorAt(const TuplePhysicalLayout& layout,
                                                            std::span<const std::byte> tuple,
                                                            std::size_t column_index) {
    const auto* column = layout.Column(column_index);
    if (column == nullptr || column->type != PhysicalType::VARCHAR) {
        ADD_FAILURE() << "requested descriptor column is not VARCHAR";
        return std::nullopt;
    }
    const auto decoded =
        DecodeVarcharDescriptor(tuple.subspan(column->fixed_offset, column->fixed_width));
    if (!decoded.Ok()) {
        ADD_FAILURE() << "VARCHAR descriptor unexpectedly failed to decode";
        return std::nullopt;
    }
    return decoded.AsOptional();
}

void WriteDescriptor(std::vector<std::byte>& tuple,
                     const TuplePhysicalLayout& layout,
                     std::size_t column_index,
                     const VarcharDescriptor& descriptor) {
    const auto* column = layout.Column(column_index);
    ASSERT_NE(column, nullptr);
    ASSERT_EQ(column->type, PhysicalType::VARCHAR);
    ASSERT_TRUE(EncodeVarcharDescriptor(
        std::span{tuple}.subspan(column->fixed_offset, column->fixed_width), descriptor));
}

void ExpectDecoded(const TuplePhysicalLayout& layout,
                   std::span<const std::byte> tuple,
                   std::size_t column_index,
                   const TupleValue& expected) {
    const auto decoded = DecodeTupleValue(layout, tuple, column_index);
    const auto* value = RequireValue(decoded, "decoded tuple value");
    if (value == nullptr) {
        return;
    }
    EXPECT_TRUE(decoded);
    EXPECT_EQ(*value, expected);
}

TEST(VarcharDescriptorCodecTest, EmitsExactLittleEndianBytesAndRoundTripsBoundaries) {
    const VarcharDescriptor descriptor{
        .payload_offset = 0x04030201U,
        .payload_length = 0x08070605U,
    };
    constexpr std::array expected{
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04},
        std::byte{0x05},
        std::byte{0x06},
        std::byte{0x07},
        std::byte{0x08},
    };
    std::array<std::byte, VARCHAR_DESCRIPTOR_ENCODED_SIZE> encoded{};
    ASSERT_TRUE(EncodeVarcharDescriptor(encoded, descriptor));
    EXPECT_EQ(encoded, expected);

    const auto decoded = DecodeVarcharDescriptor(encoded);
    const auto* decoded_descriptor = RequireValue(decoded, "VARCHAR descriptor");
    if (decoded_descriptor == nullptr) {
        return;
    }
    EXPECT_EQ(*decoded_descriptor, descriptor);

    for (const auto boundary : {
             VarcharDescriptor{},
             VarcharDescriptor{
                 .payload_offset = std::numeric_limits<std::uint32_t>::max(),
                 .payload_length = std::numeric_limits<std::uint32_t>::max(),
             },
         }) {
        ASSERT_TRUE(EncodeVarcharDescriptor(encoded, boundary));
        const auto boundary_decoded = DecodeVarcharDescriptor(encoded);
        const auto* decoded_boundary =
            RequireValue(boundary_decoded, "boundary VARCHAR descriptor");
        if (decoded_boundary == nullptr) {
            return;
        }
        EXPECT_EQ(*decoded_boundary, boundary);
    }
}

TEST(VarcharDescriptorCodecTest, SupportsUnalignedSpansAndRejectsUndersizedBuffersAtomically) {
    constexpr auto padding = std::byte{0xA5};
    const VarcharDescriptor descriptor{.payload_offset = 57, .payload_length = 11};
    std::array<std::byte, VARCHAR_DESCRIPTOR_ENCODED_SIZE + 2> buffer{};
    buffer.fill(padding);
    auto unaligned = std::span{buffer}.subspan(1, VARCHAR_DESCRIPTOR_ENCODED_SIZE);

    ASSERT_TRUE(EncodeVarcharDescriptor(unaligned, descriptor));
    EXPECT_EQ(buffer.front(), padding);
    EXPECT_EQ(buffer.back(), padding);
    const auto decoded = DecodeVarcharDescriptor(unaligned);
    const auto* decoded_descriptor = RequireValue(decoded, "unaligned VARCHAR descriptor");
    if (decoded_descriptor == nullptr) {
        return;
    }
    EXPECT_EQ(*decoded_descriptor, descriptor);

    std::array<std::byte, VARCHAR_DESCRIPTOR_ENCODED_SIZE> undersized_buffer{};
    undersized_buffer.fill(padding);
    const auto original = undersized_buffer;
    auto undersized = std::span{undersized_buffer}.first(VARCHAR_DESCRIPTOR_ENCODED_SIZE - 1U);
    EXPECT_EQ(EncodeVarcharDescriptor(undersized, descriptor).Error(),
              VarcharDescriptorCodecError::BUFFER_TOO_SMALL);
    EXPECT_EQ(undersized_buffer, original);
    EXPECT_EQ(DecodeVarcharDescriptor(undersized).Error(),
              VarcharDescriptorCodecError::BUFFER_TOO_SMALL);
}

TEST(VarlenTupleCodecTest, EncodesSingleVarcharWithExactDescriptorAndPayload) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{7});
    const auto* layout = RequireValue(built, "single VARCHAR layout");
    if (layout == nullptr) {
        return;
    }
    const std::array values{TupleValue{Varchar("abc")}};

    const auto encoded = EncodeTuple(*layout, TupleVersionMetadata{.xmin = TxnId{5}}, values);
    const auto* tuple = RequireValue(encoded, "single VARCHAR tuple");
    if (tuple == nullptr) {
        return;
    }
    ASSERT_EQ(layout->VarlenPayloadOffset(), 57U);
    ASSERT_EQ(tuple->size(), 60U);
    EXPECT_EQ((*tuple)[TUPLE_HEADER_ENCODED_SIZE], std::byte{0x00});
    constexpr std::array expected_descriptor{
        std::byte{0x39},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0x03},
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0x00},
    };
    EXPECT_TRUE(std::ranges::equal(std::span{*tuple}.subspan(49, 8), expected_descriptor));
    EXPECT_TRUE(std::ranges::equal(std::span{*tuple}.subspan(57), Varchar("abc").bytes));

    const auto header = DecodeTupleHeader(*tuple);
    const auto* decoded_header = RequireValue(header, "single VARCHAR tuple header");
    if (decoded_header == nullptr) {
        return;
    }
    EXPECT_EQ(decoded_header->tuple_flags, TUPLE_FLAG_HAS_VARLEN);
    EXPECT_TRUE(ValidateTuple(*layout, *tuple));

    const auto decoded = DecodeTupleValue(*layout, *tuple, 0);
    const auto* decoded_value = RequireValue(decoded, "single VARCHAR value");
    if (decoded_value == nullptr) {
        return;
    }
    const auto* varchar = std::get_if<VarcharValue>(decoded_value);
    ASSERT_NE(varchar, nullptr);
    EXPECT_TRUE(std::ranges::equal(varchar->bytes, Varchar("abc").bytes));
    EXPECT_EQ(varchar->bytes.data(), tuple->data() + layout->VarlenPayloadOffset());
}

TEST(VarlenTupleCodecTest, PacksMultiplePayloadsInSchemaOrderWithoutGaps) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{1});
    const auto* layout = RequireValue(built, "multiple VARCHAR layout");
    if (layout == nullptr) {
        return;
    }
    constexpr std::array binary{std::byte{0x00}, std::byte{0xFF}};
    const std::array values{
        TupleValue{Varchar("abc")},
        TupleValue{Varchar("")},
        TupleValue{VarcharValue{.bytes = binary}},
    };

    const auto encoded = EncodeTuple(*layout, TupleVersionMetadata{}, values);
    const auto* tuple = RequireValue(encoded, "multiple VARCHAR tuple");
    if (tuple == nullptr) {
        return;
    }
    ASSERT_EQ(layout->VarlenPayloadOffset(), 73U);
    ASSERT_EQ(tuple->size(), 78U);
    const auto first_descriptor =
        std::optional{VarcharDescriptor{.payload_offset = 73, .payload_length = 3}};
    const auto empty_descriptor =
        std::optional{VarcharDescriptor{.payload_offset = 76, .payload_length = 0}};
    const auto binary_descriptor =
        std::optional{VarcharDescriptor{.payload_offset = 76, .payload_length = 2}};
    EXPECT_EQ(DescriptorAt(*layout, *tuple, 0), first_descriptor);
    EXPECT_EQ(DescriptorAt(*layout, *tuple, 1), empty_descriptor);
    EXPECT_EQ(DescriptorAt(*layout, *tuple, 2), binary_descriptor);
    EXPECT_TRUE(std::ranges::equal(std::span{*tuple}.subspan(73, 3), Varchar("abc").bytes));
    EXPECT_TRUE(std::ranges::equal(std::span{*tuple}.subspan(76, 2), binary));
    EXPECT_TRUE(ValidateTuple(*layout, *tuple));

    for (std::size_t index = 0; index < values.size(); ++index) {
        ExpectDecoded(*layout, *tuple, index, values[index]);
    }
}

TEST(VarlenTupleCodecTest, EncodesExactMixedFixedVarlenAndNullLayout) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = true},
        PhysicalColumnSpec{.type = PhysicalType::BOOLEAN, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::INT64, .nullable = true},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{12});
    const auto* layout = RequireValue(built, "mixed varlen layout");
    if (layout == nullptr) {
        return;
    }
    const std::array values{
        TupleValue{std::int32_t{0x01020304}},
        TupleValue{std::monostate{}},
        TupleValue{true},
        TupleValue{Varchar("xyz")},
        TupleValue{std::monostate{}},
    };

    const auto encoded = EncodeTuple(*layout, TupleVersionMetadata{.xmin = TxnId{19}}, values);
    const auto* tuple = RequireValue(encoded, "mixed varlen tuple");
    if (tuple == nullptr) {
        return;
    }
    ASSERT_EQ(layout->FixedAreaOffset(), 49U);
    ASSERT_EQ(layout->Columns()[0].fixed_offset, 49U);
    ASSERT_EQ(layout->Columns()[1].fixed_offset, 53U);
    ASSERT_EQ(layout->Columns()[2].fixed_offset, 61U);
    ASSERT_EQ(layout->Columns()[3].fixed_offset, 62U);
    ASSERT_EQ(layout->Columns()[4].fixed_offset, 70U);
    ASSERT_EQ(layout->VarlenPayloadOffset(), 78U);
    ASSERT_EQ(tuple->size(), 81U);
    EXPECT_EQ((*tuple)[48], std::byte{0x12});
    EXPECT_TRUE(std::ranges::equal(
        std::span{*tuple}.subspan(49, 4),
        std::array{std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01}}));
    const auto null_descriptor = std::optional{VarcharDescriptor{}};
    const auto payload_descriptor =
        std::optional{VarcharDescriptor{.payload_offset = 78, .payload_length = 3}};
    EXPECT_EQ(DescriptorAt(*layout, *tuple, 1), null_descriptor);
    EXPECT_EQ((*tuple)[61], std::byte{0x01});
    EXPECT_EQ(DescriptorAt(*layout, *tuple, 3), payload_descriptor);
    EXPECT_TRUE(std::ranges::all_of(std::span{*tuple}.subspan(70, 8),
                                    [](std::byte value) { return value == std::byte{0}; }));
    EXPECT_TRUE(std::ranges::equal(std::span{*tuple}.subspan(78), Varchar("xyz").bytes));

    const auto validation = ValidateTuple(*layout, *tuple);
    const auto* header = RequireValue(validation, "mixed varlen tuple header");
    if (header == nullptr) {
        return;
    }
    EXPECT_EQ(header->tuple_flags, TUPLE_FLAG_HAS_NULLS | TUPLE_FLAG_HAS_VARLEN);
    for (std::size_t index = 0; index < values.size(); ++index) {
        ExpectDecoded(*layout, *tuple, index, values[index]);
    }
}

TEST(VarlenTupleCodecTest, DistinguishesNullAndEmptyAndSetsVarlenForAllNullSchema) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = true},
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = true},
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = true},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{1});
    const auto* layout = RequireValue(built, "NULL and empty VARCHAR layout");
    if (layout == nullptr) {
        return;
    }
    const std::array values{
        TupleValue{std::monostate{}},
        TupleValue{Varchar("")},
        TupleValue{std::monostate{}},
    };

    const auto encoded = EncodeTuple(*layout, TupleVersionMetadata{}, values);
    const auto* tuple = RequireValue(encoded, "NULL and empty VARCHAR tuple");
    if (tuple == nullptr) {
        return;
    }
    ASSERT_EQ(tuple->size(), layout->MinimumTupleSize());
    EXPECT_EQ((*tuple)[48], std::byte{0x05});
    const auto null_descriptor = std::optional{VarcharDescriptor{}};
    const auto empty_descriptor = std::optional{VarcharDescriptor{
        .payload_offset = static_cast<std::uint32_t>(layout->VarlenPayloadOffset()),
        .payload_length = 0,
    }};
    EXPECT_EQ(DescriptorAt(*layout, *tuple, 0), null_descriptor);
    EXPECT_EQ(DescriptorAt(*layout, *tuple, 1), empty_descriptor);
    EXPECT_EQ(DescriptorAt(*layout, *tuple, 2), null_descriptor);
    ExpectDecoded(*layout, *tuple, 0, TupleValue{std::monostate{}});
    ExpectDecoded(*layout, *tuple, 1, TupleValue{Varchar("")});

    std::array<TupleValue, 3> all_null{};
    all_null.fill(TupleValue{std::monostate{}});
    const auto all_null_encoded = EncodeTuple(*layout, TupleVersionMetadata{}, all_null);
    const auto* all_null_tuple = RequireValue(all_null_encoded, "all-NULL VARCHAR tuple");
    if (all_null_tuple == nullptr) {
        return;
    }
    EXPECT_EQ(all_null_tuple->size(), layout->MinimumTupleSize());
    const auto header = DecodeTupleHeader(*all_null_tuple);
    const auto* decoded_header = RequireValue(header, "all-NULL VARCHAR header");
    if (decoded_header == nullptr) {
        return;
    }
    EXPECT_EQ(decoded_header->tuple_flags, TUPLE_FLAG_HAS_NULLS | TUPLE_FLAG_HAS_VARLEN);
    for (std::size_t index = 0; index < all_null.size(); ++index) {
        EXPECT_EQ(DescriptorAt(*layout, *all_null_tuple, index), null_descriptor);
    }
}

TEST(VarlenTupleCodecTest, RejectsTypeCountAndNullabilityErrorsWithoutOutputTuple) {
    constexpr std::array varchar_column{
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
    };
    const auto varchar_built = BuildTuplePhysicalLayout(varchar_column, SchemaVer{1});
    const auto* varchar_layout = RequireValue(varchar_built, "validation VARCHAR layout");
    if (varchar_layout == nullptr) {
        return;
    }

    const std::array wrong_type{TupleValue{std::int32_t{1}}};
    const auto mismatch = EncodeTuple(*varchar_layout, TupleVersionMetadata{}, wrong_type);
    EXPECT_FALSE(mismatch.Ok());
    EXPECT_EQ(mismatch.Error().code, TupleCodecError::TYPE_MISMATCH);
    EXPECT_EQ(mismatch.Error().column_index, 0U);

    const std::array null_value{TupleValue{std::monostate{}}};
    const auto null_rejected = EncodeTuple(*varchar_layout, TupleVersionMetadata{}, null_value);
    EXPECT_FALSE(null_rejected.Ok());
    EXPECT_EQ(null_rejected.Error().code, TupleCodecError::NULL_NOT_ALLOWED);
    EXPECT_EQ(null_rejected.Error().column_index, 0U);

    const auto count_mismatch = EncodeTuple(*varchar_layout, TupleVersionMetadata{}, {});
    EXPECT_FALSE(count_mismatch.Ok());
    EXPECT_EQ(count_mismatch.Error().code, TupleCodecError::COLUMN_COUNT_MISMATCH);

    constexpr std::array fixed_column{
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = false},
    };
    const auto fixed_built = BuildTuplePhysicalLayout(fixed_column, SchemaVer{1});
    const auto* fixed_layout = RequireValue(fixed_built, "VARCHAR mismatch fixed layout");
    if (fixed_layout == nullptr) {
        return;
    }
    const std::array varchar_value{TupleValue{Varchar("1")}};
    const auto fixed_mismatch = EncodeTuple(*fixed_layout, TupleVersionMetadata{}, varchar_value);
    EXPECT_FALSE(fixed_mismatch.Ok());
    EXPECT_EQ(fixed_mismatch.Error().code, TupleCodecError::TYPE_MISMATCH);
    EXPECT_EQ(fixed_mismatch.Error().column_index, 0U);
}

TEST(VarlenTupleCodecTest, EnforcesVarcharAndInlineTupleSizeLimitsBeforeOutputAllocation) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{1});
    const auto* layout = RequireValue(built, "VARCHAR limit layout");
    if (layout == nullptr) {
        return;
    }
    const std::size_t exact_payload = HEAP_PAGE_MAX_RAW_TUPLE_SIZE - layout->MinimumTupleSize();
    std::vector<std::byte> exact_bytes(exact_payload, std::byte{0xA5});
    const std::array exact_value{TupleValue{VarcharValue{.bytes = exact_bytes}}};
    const auto exact = EncodeTuple(*layout, TupleVersionMetadata{}, exact_value);
    const auto* exact_tuple = RequireValue(exact, "maximum-size VARCHAR tuple");
    if (exact_tuple == nullptr) {
        return;
    }
    EXPECT_EQ(exact_tuple->size(), HEAP_PAGE_MAX_RAW_TUPLE_SIZE);
    EXPECT_TRUE(ValidateTuple(*layout, *exact_tuple));

    std::vector<std::byte> oversized_bytes(exact_payload + 1U, std::byte{0x5A});
    const std::array oversized_value{TupleValue{VarcharValue{.bytes = oversized_bytes}}};
    const auto oversized = EncodeTuple(*layout, TupleVersionMetadata{}, oversized_value);
    EXPECT_FALSE(oversized.Ok());
    EXPECT_EQ(oversized.Error().code, TupleCodecError::TUPLE_TOO_LARGE);

    if constexpr (std::numeric_limits<std::size_t>::max() >
                  std::numeric_limits<std::uint32_t>::max()) {
        const std::array unrepresentable{
            VarlenValueSize{
                .length = static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) + 1U,
                .is_null = false,
            },
        };
        EXPECT_EQ(layout->PlanTupleSize(unrepresentable).Error().code,
                  TupleSizePlanningError::VARCHAR_LENGTH_TOO_LARGE);
    }
}

TEST(VarlenTupleCodecTest, RejectsDescriptorPackingFlagBitmapAndLengthCorruption) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = true},
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{4});
    const auto* layout = RequireValue(built, "corruption VARCHAR layout");
    if (layout == nullptr) {
        return;
    }
    const std::array values{TupleValue{Varchar("abc")}, TupleValue{Varchar("de")}};
    const auto encoded = EncodeTuple(*layout, TupleVersionMetadata{}, values);
    const auto* valid_tuple = RequireValue(encoded, "corruption base tuple");
    if (valid_tuple == nullptr) {
        return;
    }
    const auto payload_start = static_cast<std::uint32_t>(layout->VarlenPayloadOffset());

    auto before_payload = *valid_tuple;
    WriteDescriptor(
        before_payload, *layout, 0, {.payload_offset = payload_start - 1U, .payload_length = 3});
    EXPECT_EQ(ValidateTuple(*layout, before_payload).Error().code,
              TupleCodecError::INVALID_VARLEN_DESCRIPTOR);

    auto beyond_tuple = *valid_tuple;
    WriteDescriptor(
        beyond_tuple,
        *layout,
        0,
        {.payload_offset = static_cast<std::uint32_t>(beyond_tuple.size()), .payload_length = 1});
    EXPECT_EQ(ValidateTuple(*layout, beyond_tuple).Error().code,
              TupleCodecError::INVALID_VARLEN_DESCRIPTOR);

    auto overflow = *valid_tuple;
    WriteDescriptor(
        overflow,
        *layout,
        0,
        {.payload_offset = std::numeric_limits<std::uint32_t>::max() - 1U, .payload_length = 4});
    EXPECT_EQ(ValidateTuple(*layout, overflow).Error().code,
              TupleCodecError::INVALID_VARLEN_DESCRIPTOR);

    auto overlap = *valid_tuple;
    WriteDescriptor(overlap, *layout, 0, {.payload_offset = payload_start, .payload_length = 4});
    EXPECT_EQ(ValidateTuple(*layout, overlap).Error().code,
              TupleCodecError::VARLEN_OFFSET_MISMATCH);

    auto gap = *valid_tuple;
    WriteDescriptor(gap, *layout, 0, {.payload_offset = payload_start, .payload_length = 2});
    EXPECT_EQ(ValidateTuple(*layout, gap).Error().code, TupleCodecError::VARLEN_OFFSET_MISMATCH);

    auto schema_order = *valid_tuple;
    WriteDescriptor(
        schema_order, *layout, 0, {.payload_offset = payload_start + 3U, .payload_length = 2});
    EXPECT_EQ(ValidateTuple(*layout, schema_order).Error().code,
              TupleCodecError::VARLEN_OFFSET_MISMATCH);

    auto wrong_varlen_flag = *valid_tuple;
    ASSERT_EQ(EncodeLittleEndian(std::span{wrong_varlen_flag}.subspan(TUPLE_HEADER_FLAGS_OFFSET,
                                                                      sizeof(TupleFlags)),
                                 TupleFlags{0}),
              true);
    EXPECT_EQ(ValidateTuple(*layout, wrong_varlen_flag).Error().code,
              TupleCodecError::VARLEN_FLAG_MISMATCH);

    auto malformed_bitmap = *valid_tuple;
    malformed_bitmap[TUPLE_HEADER_ENCODED_SIZE] = std::byte{0x80};
    EXPECT_EQ(ValidateTuple(*layout, malformed_bitmap).Error().code,
              TupleCodecError::MALFORMED_TUPLE);

    auto not_null_bitmap = *valid_tuple;
    not_null_bitmap[TUPLE_HEADER_ENCODED_SIZE] = std::byte{0x02};
    ASSERT_TRUE(EncodeLittleEndian(
        std::span{not_null_bitmap}.subspan(TUPLE_HEADER_FLAGS_OFFSET, sizeof(TupleFlags)),
        static_cast<TupleFlags>(TUPLE_FLAG_HAS_NULLS | TUPLE_FLAG_HAS_VARLEN)));
    EXPECT_EQ(ValidateTuple(*layout, not_null_bitmap).Error().code,
              TupleCodecError::NULL_NOT_ALLOWED);

    auto trailing = *valid_tuple;
    trailing.push_back(std::byte{0xA5});
    EXPECT_EQ(ValidateTuple(*layout, trailing).Error().code, TupleCodecError::TRAILING_BYTES);

    auto truncated = *valid_tuple;
    truncated.pop_back();
    EXPECT_EQ(ValidateTuple(*layout, truncated).Error().code,
              TupleCodecError::INVALID_VARLEN_DESCRIPTOR);

    const std::array null_values{TupleValue{std::monostate{}}, TupleValue{Varchar("de")}};
    const auto null_encoded = EncodeTuple(*layout, TupleVersionMetadata{}, null_values);
    const auto* null_tuple = RequireValue(null_encoded, "NULL descriptor tuple");
    if (null_tuple == nullptr) {
        return;
    }
    auto noncanonical_null = *null_tuple;
    WriteDescriptor(
        noncanonical_null, *layout, 0, {.payload_offset = payload_start, .payload_length = 0});
    EXPECT_EQ(ValidateTuple(*layout, noncanonical_null).Error().code,
              TupleCodecError::INVALID_VARLEN_DESCRIPTOR);
}

TEST(VarlenTupleCodecTest, ValidatesSchemaVersionAndFixedOnlyExactLength) {
    constexpr std::array varchar_column{
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
    };
    const auto first = BuildTuplePhysicalLayout(varchar_column, SchemaVer{10});
    const auto second = BuildTuplePhysicalLayout(varchar_column, SchemaVer{11});
    const auto* first_layout = RequireValue(first, "first schema layout");
    const auto* second_layout = RequireValue(second, "second schema layout");
    if (first_layout == nullptr || second_layout == nullptr) {
        return;
    }
    const std::array value{TupleValue{Varchar("x")}};
    const auto encoded = EncodeTuple(*first_layout, TupleVersionMetadata{}, value);
    const auto* tuple = RequireValue(encoded, "schema-version tuple");
    if (tuple == nullptr) {
        return;
    }
    EXPECT_TRUE(ValidateTuple(*first_layout, *tuple));
    EXPECT_EQ(ValidateTuple(*second_layout, *tuple).Error().code,
              TupleCodecError::SCHEMA_VERSION_MISMATCH);

    constexpr std::array fixed_column{
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = false},
    };
    const auto fixed = BuildTuplePhysicalLayout(fixed_column, SchemaVer{1});
    const auto* fixed_layout = RequireValue(fixed, "fixed exact-length layout");
    if (fixed_layout == nullptr) {
        return;
    }
    const std::array fixed_value{TupleValue{std::int32_t{7}}};
    const auto fixed_encoded = EncodeTuple(*fixed_layout, TupleVersionMetadata{}, fixed_value);
    const auto* fixed_tuple = RequireValue(fixed_encoded, "fixed exact-length tuple");
    if (fixed_tuple == nullptr) {
        return;
    }
    auto trailing = *fixed_tuple;
    trailing.push_back(std::byte{0x00});
    EXPECT_EQ(ValidateTuple(*fixed_layout, trailing).Error().code, TupleCodecError::TRAILING_BYTES);
}

TEST(VarlenTupleCodecTest, ComposesWithHeapPageAndPreservesBorrowedVarcharView) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::INT64, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::BOOLEAN, .nullable = false},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{2});
    const auto* layout = RequireValue(built, "HeapPage varlen layout");
    if (layout == nullptr) {
        return;
    }
    const std::array values{
        TupleValue{std::int64_t{-9}},
        TupleValue{Varchar("heap bytes")},
        TupleValue{true},
    };
    const auto encoded = EncodeTuple(*layout, TupleVersionMetadata{.xmin = TxnId{2}}, values);
    const auto* tuple = RequireValue(encoded, "HeapPage varlen tuple");
    if (tuple == nullptr) {
        return;
    }

    const PageId page_id{.file_id = FileId{15}, .page_no = PageNo{3}};
    Page page{page_id};
    HeapPage heap_page{page, HeapTupleSchemaHistory{std::span{layout, 1}}};
    ASSERT_TRUE(heap_page.Initialize());
    const auto inserted = heap_page.Insert(*tuple);
    const auto* rid = RequireValue(inserted, "HeapPage varlen RID");
    if (rid == nullptr) {
        return;
    }
    const auto stored = heap_page.TupleBytes(rid->slot);
    const auto* stored_tuple = RequireValue(stored, "HeapPage stored varlen tuple");
    if (stored_tuple == nullptr) {
        return;
    }
    EXPECT_TRUE(ValidateTuple(*layout, *stored_tuple));
    for (std::size_t index = 0; index < values.size(); ++index) {
        ExpectDecoded(*layout, *stored_tuple, index, values[index]);
    }
    const auto decoded = DecodeTupleValue(*layout, *stored_tuple, 1);
    const auto* decoded_value = RequireValue(decoded, "HeapPage VARCHAR view");
    if (decoded_value == nullptr) {
        return;
    }
    const auto* varchar = std::get_if<VarcharValue>(decoded_value);
    ASSERT_NE(varchar, nullptr);
    EXPECT_GE(varchar->bytes.data(), stored_tuple->data());
    EXPECT_LT(varchar->bytes.data(), stored_tuple->data() + stored_tuple->size());
}

TEST(VarlenTuplePersistenceTest, SurvivesHeapPageFileWriteReopenAndDecode) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.Valid());
    const auto path = temporary_directory.File("mixed.heap");
    constexpr FileId file_id{71};
    const FileSuperblock superblock{
        .file_kind = FileKind::HEAP,
        .file_id = file_id,
        .object_id = 1701,
        .creation_epoch = 2701,
    };
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::TIMESTAMP, .nullable = false},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{6});
    const auto* layout = RequireValue(built, "persisted varlen layout");
    if (layout == nullptr) {
        return;
    }
    const std::array values{
        TupleValue{std::int32_t{42}},
        TupleValue{Varchar("persisted bytes")},
        TupleValue{TimestampPhysicalValue{.value = -1234567}},
    };
    const auto encoded = EncodeTuple(*layout, TupleVersionMetadata{.xmin = TxnId{33}}, values);
    const auto* tuple = RequireValue(encoded, "persisted varlen tuple");
    if (tuple == nullptr) {
        return;
    }

    DiskManager manager;
    PageId page_id{};
    SlotId slot_id = INVALID_SLOT_ID;
    {
        auto created = PageFile::Create(manager, path, superblock);
        auto* page_file = RequireValue(created, "created heap PageFile");
        if (page_file == nullptr) {
            return;
        }
        const auto allocation = page_file->AllocatePage();
        const auto* allocated_page = RequireValue(allocation, "allocated heap page");
        if (allocated_page == nullptr) {
            return;
        }
        page_id = *allocated_page;

        Page page{page_id};
        HeapPage heap_page{page, HeapTupleSchemaHistory{std::span{layout, 1}}};
        ASSERT_TRUE(heap_page.Initialize());
        const auto inserted = heap_page.Insert(*tuple);
        const auto* rid = RequireValue(inserted, "persisted tuple RID");
        if (rid == nullptr) {
            return;
        }
        slot_id = rid->slot;
        ASSERT_TRUE(manager.WritePage(page.Id(), page.Bytes()));
        ASSERT_TRUE(manager.SyncFile(file_id));
    }

    auto reopened = PageFile::Open(manager, path, file_id, FileKind::HEAP, superblock.object_id);
    if (!reopened.Ok()) {
        ADD_FAILURE() << "heap PageFile unexpectedly failed to reopen";
        return;
    }
    Page read_page{page_id};
    ASSERT_TRUE(manager.ReadPage(page_id, read_page.Bytes()));
    HeapPage read_heap_page{read_page, HeapTupleSchemaHistory{std::span{layout, 1}}};
    EXPECT_TRUE(read_heap_page.Validate());
    const auto stored = read_heap_page.TupleBytes(slot_id);
    const auto* stored_tuple = RequireValue(stored, "persisted stored tuple");
    if (stored_tuple == nullptr) {
        return;
    }
    EXPECT_TRUE(ValidateTuple(*layout, *stored_tuple));
    for (std::size_t index = 0; index < values.size(); ++index) {
        ExpectDecoded(*layout, *stored_tuple, index, values[index]);
    }
}

} // namespace
} // namespace dblusblus::storage
