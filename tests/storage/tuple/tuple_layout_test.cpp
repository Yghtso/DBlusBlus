#include "storage/heap/heap_page_format.h"
#include "storage/tuple/tuple_header.h"
#include "storage/tuple/tuple_layout.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <vector>

namespace dblusblus::storage {
namespace {

static_assert(BOOLEAN_PHYSICAL_SIZE == 1);
static_assert(INT32_PHYSICAL_SIZE == 4);
static_assert(INT64_PHYSICAL_SIZE == 8);
static_assert(FLOAT64_PHYSICAL_SIZE == 8);
static_assert(DATE_PHYSICAL_SIZE == 4);
static_assert(TIMESTAMP_PHYSICAL_SIZE == 8);
static_assert(VARCHAR_DESCRIPTOR_OFFSET_OFFSET == 0);
static_assert(VARCHAR_DESCRIPTOR_LENGTH_OFFSET == 4);
static_assert(VARCHAR_DESCRIPTOR_ENCODED_SIZE == 8);

TEST(NullBitmapTest, CalculatesOneBitPerColumnWithCheckedPersistedSize) {
    EXPECT_EQ(NullBitmapBytes(0), std::optional<std::uint16_t>{0});
    EXPECT_EQ(NullBitmapBytes(1), std::optional<std::uint16_t>{1});
    EXPECT_EQ(NullBitmapBytes(8), std::optional<std::uint16_t>{1});
    EXPECT_EQ(NullBitmapBytes(9), std::optional<std::uint16_t>{2});
    EXPECT_EQ(NullBitmapBytes(15), std::optional<std::uint16_t>{2});
    EXPECT_EQ(NullBitmapBytes(16), std::optional<std::uint16_t>{2});
    EXPECT_EQ(NullBitmapBytes(17), std::optional<std::uint16_t>{3});

    constexpr std::size_t max_columns =
        static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) * 8U;
    EXPECT_EQ(NullBitmapBytes(max_columns),
              std::optional<std::uint16_t>{std::numeric_limits<std::uint16_t>::max()});
    EXPECT_FALSE(NullBitmapBytes(max_columns + 1U).has_value());
    EXPECT_FALSE(NullBitmapBytes(std::numeric_limits<std::size_t>::max()).has_value());
}

TEST(NullBitmapTest, UsesExactLsbFirstBitOrderingAcrossByteBoundaries) {
    std::array<std::byte, 2> bitmap{};

    for (const std::size_t column : {0U, 7U, 8U, 15U}) {
        EXPECT_TRUE(SetNull(bitmap, 16, column));
    }
    EXPECT_EQ(bitmap[0], std::byte{0x81});
    EXPECT_EQ(bitmap[1], std::byte{0x81});

    for (std::size_t column = 0; column < 16; ++column) {
        const auto result = IsNull(bitmap, 16, column);
        ASSERT_TRUE(result);
        const bool expected = column == 0 || column == 7 || column == 8 || column == 15;
        EXPECT_EQ(result.AsOptional(), expected);
    }
}

TEST(NullBitmapTest, SetAndClearPreserveUnrelatedBits) {
    std::array bitmap{std::byte{0xAA}, std::byte{0x55}};

    EXPECT_TRUE(SetNull(bitmap, 16, 0));
    EXPECT_EQ(bitmap[0], std::byte{0xAB});
    EXPECT_EQ(bitmap[1], std::byte{0x55});

    EXPECT_TRUE(ClearNull(bitmap, 16, 1));
    EXPECT_EQ(bitmap[0], std::byte{0xA9});
    EXPECT_EQ(bitmap[1], std::byte{0x55});

    EXPECT_TRUE(ClearNull(bitmap, 16, 8));
    EXPECT_EQ(bitmap[0], std::byte{0xA9});
    EXPECT_EQ(bitmap[1], std::byte{0x54});
}

TEST(NullBitmapTest, RejectsOutOfRangeAndUndersizedAccessWithoutMutation) {
    std::array<std::byte, 1> bitmap{std::byte{0x5A}};
    const auto original = bitmap;

    EXPECT_EQ(SetNull(bitmap, 8, 8).Error(), NullBitmapError::COLUMN_OUT_OF_RANGE);
    EXPECT_EQ(ClearNull(bitmap, 0, 0).Error(), NullBitmapError::COLUMN_OUT_OF_RANGE);
    EXPECT_EQ(SetNull(bitmap, 9, 0).Error(), NullBitmapError::BITMAP_TOO_SMALL);
    EXPECT_EQ(ClearNull(bitmap, 9, 8).Error(), NullBitmapError::BITMAP_TOO_SMALL);
    EXPECT_EQ(bitmap, original);

    EXPECT_EQ(IsNull(bitmap, 8, 8).Error(), NullBitmapError::COLUMN_OUT_OF_RANGE);
    EXPECT_EQ(IsNull(bitmap, 9, 0).Error(), NullBitmapError::BITMAP_TOO_SMALL);

    constexpr std::size_t oversized_column_count =
        (static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) * 8U) + 1U;
    EXPECT_EQ(IsNull(bitmap, oversized_column_count, 0).Error(),
              NullBitmapError::COLUMN_COUNT_TOO_LARGE);
    EXPECT_EQ(SetNull(bitmap, oversized_column_count, 0).Error(),
              NullBitmapError::COLUMN_COUNT_TOO_LARGE);
    EXPECT_EQ(bitmap, original);
}

TEST(TuplePhysicalLayoutTest, PinsEveryLockedPhysicalWidth) {
    struct WidthCase {
        PhysicalType type;
        std::size_t width;
    };
    constexpr std::array cases{
        WidthCase{.type = PhysicalType::BOOLEAN, .width = 1},
        WidthCase{.type = PhysicalType::INT32, .width = 4},
        WidthCase{.type = PhysicalType::INT64, .width = 8},
        WidthCase{.type = PhysicalType::FLOAT64, .width = 8},
        WidthCase{.type = PhysicalType::DATE, .width = 4},
        WidthCase{.type = PhysicalType::TIMESTAMP, .width = 8},
        WidthCase{.type = PhysicalType::VARCHAR, .width = 8},
    };

    for (const auto& width_case : cases) {
        EXPECT_EQ(PhysicalTypeWidth(width_case.type), std::optional<std::size_t>{width_case.width});

        const std::array columns{PhysicalColumnSpec{.type = width_case.type, .nullable = false}};
        const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{1});
        if (!built.Ok()) {
            ADD_FAILURE() << "one-column physical layout unexpectedly failed";
            continue;
        }
        const auto& layout = *built;
        const auto* column = layout.Column(0);
        ASSERT_NE(column, nullptr);
        EXPECT_EQ(column->fixed_offset, TUPLE_HEADER_ENCODED_SIZE + 1U);
        EXPECT_EQ(column->fixed_width, width_case.width);
        EXPECT_EQ(column->null_bit_index, 0U);
        EXPECT_EQ(layout.FixedAreaSize(), width_case.width);
        EXPECT_EQ(layout.MinimumTupleSize(), TUPLE_HEADER_ENCODED_SIZE + 1U + width_case.width);
    }

    EXPECT_FALSE(PhysicalTypeWidth(static_cast<PhysicalType>(0xFFU)).has_value());
}

TEST(TuplePhysicalLayoutTest, PacksMixedFixedAreaWithoutAlignmentPadding) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = true},
        PhysicalColumnSpec{.type = PhysicalType::INT64, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::BOOLEAN, .nullable = false},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{7});
    if (!built.Ok()) {
        ADD_FAILURE() << "mixed physical layout unexpectedly failed";
        return;
    }
    const auto& layout = *built;

    ASSERT_EQ(layout.Columns().size(), 4U);
    EXPECT_EQ(layout.SchemaVersion(), SchemaVer{7});
    EXPECT_EQ(layout.NullBitmapSize(), 1U);
    EXPECT_EQ(layout.FixedAreaOffset(), 49U);
    EXPECT_EQ(layout.Columns()[0],
              (PhysicalColumnLayout{
                  .type = PhysicalType::INT32,
                  .nullable = false,
                  .fixed_offset = 49,
                  .fixed_width = 4,
                  .null_bit_index = 0,
              }));
    EXPECT_EQ(layout.Columns()[1],
              (PhysicalColumnLayout{
                  .type = PhysicalType::VARCHAR,
                  .nullable = true,
                  .fixed_offset = 53,
                  .fixed_width = 8,
                  .null_bit_index = 1,
              }));
    EXPECT_EQ(layout.Columns()[2].fixed_offset, 61U);
    EXPECT_EQ(layout.Columns()[2].fixed_width, 8U);
    EXPECT_EQ(layout.Columns()[2].null_bit_index, 2U);
    EXPECT_EQ(layout.Columns()[3].fixed_offset, 69U);
    EXPECT_EQ(layout.Columns()[3].fixed_width, 1U);
    EXPECT_EQ(layout.Columns()[3].null_bit_index, 3U);
    EXPECT_EQ(layout.FixedAreaSize(), 21U);
    EXPECT_EQ(layout.VarlenPayloadOffset(), 70U);
    EXPECT_EQ(layout.MinimumTupleSize(), 70U);
    EXPECT_EQ(layout.VarlenColumnCount(), 1U);
    EXPECT_TRUE(layout.HasNullableColumns());
    EXPECT_TRUE(layout.HasVarlenColumns());
    EXPECT_EQ(layout.Column(4), nullptr);
}

TEST(TuplePhysicalLayoutTest, AllocatesOneNullBitForEveryColumnDeterministically) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::BOOLEAN, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::DATE, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::INT64, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::FLOAT64, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::TIMESTAMP, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::BOOLEAN, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
    };

    const auto first = BuildTuplePhysicalLayout(columns, SchemaVer{0});
    const auto second = BuildTuplePhysicalLayout(columns, SchemaVer{0});
    if (!first.Ok() || !second.Ok()) {
        ADD_FAILURE() << "deterministic physical layouts unexpectedly failed";
        return;
    }
    const auto& first_layout = *first;
    const auto& second_layout = *second;
    EXPECT_EQ(first_layout, second_layout);
    EXPECT_EQ(first_layout.NullBitmapSize(), 2U);
    EXPECT_FALSE(first_layout.HasNullableColumns());
    ASSERT_EQ(first_layout.Columns().size(), columns.size());
    for (std::size_t index = 0; index < columns.size(); ++index) {
        EXPECT_EQ(first_layout.Columns()[index].null_bit_index, index);
    }
}

TEST(TuplePhysicalLayoutTest, AcceptsZeroColumnsAndAnySchemaVersion) {
    const auto built = BuildTuplePhysicalLayout(std::span<const PhysicalColumnSpec>{},
                                                std::numeric_limits<SchemaVer>::max());
    if (!built.Ok()) {
        ADD_FAILURE() << "zero-column physical layout unexpectedly failed";
        return;
    }
    const auto& layout = *built;

    EXPECT_EQ(layout.SchemaVersion(), std::numeric_limits<SchemaVer>::max());
    EXPECT_EQ(layout.ColumnCount(), 0U);
    EXPECT_EQ(layout.NullBitmapSize(), 0U);
    EXPECT_EQ(layout.FixedAreaOffset(), TUPLE_HEADER_ENCODED_SIZE);
    EXPECT_EQ(layout.FixedAreaSize(), 0U);
    EXPECT_EQ(layout.VarlenPayloadOffset(), TUPLE_HEADER_ENCODED_SIZE);
    EXPECT_EQ(layout.MinimumTupleSize(), TUPLE_HEADER_ENCODED_SIZE);
    EXPECT_FALSE(layout.HasNullableColumns());
    EXPECT_FALSE(layout.HasVarlenColumns());

    const auto planned = layout.PlanTupleSize({});
    const std::optional<TupleSizePlan> expected_plan{TupleSizePlan{
        .varlen_payload_size = 0,
        .total_size = TUPLE_HEADER_ENCODED_SIZE,
    }};
    EXPECT_EQ(planned.AsOptional(), expected_plan);
}

TEST(TuplePhysicalLayoutTest, RejectsInvalidTypesAndLayoutsLargerThanRawHeapLimit) {
    const std::array invalid{
        PhysicalColumnSpec{.type = static_cast<PhysicalType>(0xFFU), .nullable = false},
    };
    const auto invalid_result = BuildTuplePhysicalLayout(invalid, SchemaVer{1});
    EXPECT_FALSE(invalid_result.Ok());
    EXPECT_EQ(invalid_result.Error().code, TuplePhysicalLayoutBuildError::INVALID_PHYSICAL_TYPE);
    EXPECT_EQ(invalid_result.Error().column_index, 0U);

    std::array<PhysicalColumnSpec, 1000> too_large{};
    for (auto& column : too_large) {
        column = PhysicalColumnSpec{.type = PhysicalType::INT64, .nullable = false};
    }
    const auto oversized_result = BuildTuplePhysicalLayout(too_large, SchemaVer{1});
    EXPECT_FALSE(oversized_result.Ok());
    EXPECT_EQ(oversized_result.Error().code, TuplePhysicalLayoutBuildError::TUPLE_TOO_LARGE);
}

TEST(TupleSizePlanningTest, HandlesNoOneAndMultipleVarcharColumns) {
    constexpr std::array fixed_columns{
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::INT64, .nullable = false},
    };
    const auto fixed_built = BuildTuplePhysicalLayout(fixed_columns, SchemaVer{1});
    if (!fixed_built.Ok()) {
        ADD_FAILURE() << "fixed-only physical layout unexpectedly failed";
        return;
    }
    const auto& fixed_layout = *fixed_built;
    const auto fixed_plan = fixed_layout.PlanTupleSize({});
    const std::optional<TupleSizePlan> expected_fixed{TupleSizePlan{
        .varlen_payload_size = 0,
        .total_size = fixed_layout.MinimumTupleSize(),
    }};
    EXPECT_EQ(fixed_plan.AsOptional(), expected_fixed);

    constexpr std::array one_varchar{
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = true},
    };
    const auto one_built = BuildTuplePhysicalLayout(one_varchar, SchemaVer{1});
    if (!one_built.Ok()) {
        ADD_FAILURE() << "one-VARCHAR physical layout unexpectedly failed";
        return;
    }
    const auto& one_layout = *one_built;
    constexpr std::array one_value{VarlenValueSize{.length = 12, .is_null = false}};
    const std::optional<TupleSizePlan> expected_one{TupleSizePlan{
        .varlen_payload_size = 12,
        .total_size = 69,
    }};
    EXPECT_EQ(one_layout.PlanTupleSize(one_value).AsOptional(), expected_one);

    constexpr std::array mixed_columns{
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = true},
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
    };
    const auto mixed_built = BuildTuplePhysicalLayout(mixed_columns, SchemaVer{1});
    if (!mixed_built.Ok()) {
        ADD_FAILURE() << "multi-VARCHAR physical layout unexpectedly failed";
        return;
    }
    const auto& mixed_layout = *mixed_built;
    constexpr std::array values{
        VarlenValueSize{.length = 5, .is_null = false},
        VarlenValueSize{.length = 999, .is_null = true},
        VarlenValueSize{.length = 0, .is_null = false},
    };
    EXPECT_EQ(mixed_layout.MinimumTupleSize(), 77U);
    const std::optional<TupleSizePlan> expected_mixed{TupleSizePlan{
        .varlen_payload_size = 5,
        .total_size = 82,
    }};
    EXPECT_EQ(mixed_layout.PlanTupleSize(values).AsOptional(), expected_mixed);
}

TEST(TupleSizePlanningTest, RejectsCountRepresentationAndHeapLimitFailures) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = true},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{1});
    if (!built.Ok()) {
        ADD_FAILURE() << "VARCHAR physical layout unexpectedly failed";
        return;
    }
    const auto& layout = *built;
    ASSERT_EQ(layout.MinimumTupleSize(), 57U);

    EXPECT_EQ(layout.PlanTupleSize({}).Error().code,
              TupleSizePlanningError::VARLEN_VALUE_COUNT_MISMATCH);
    constexpr std::array too_many{
        VarlenValueSize{.length = 0, .is_null = false},
        VarlenValueSize{.length = 0, .is_null = false},
    };
    EXPECT_EQ(layout.PlanTupleSize(too_many).Error().code,
              TupleSizePlanningError::VARLEN_VALUE_COUNT_MISMATCH);

    constexpr std::array uint32_max{
        VarlenValueSize{
            .length = std::numeric_limits<std::uint32_t>::max(),
            .is_null = false,
        },
    };
    EXPECT_EQ(layout.PlanTupleSize(uint32_max).Error().code,
              TupleSizePlanningError::TUPLE_TOO_LARGE);

    if constexpr (std::numeric_limits<std::size_t>::max() >
                  std::numeric_limits<std::uint32_t>::max()) {
        const std::array unrepresentable{
            VarlenValueSize{
                .length = static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) + 1U,
                .is_null = false,
            },
        };
        const auto result = layout.PlanTupleSize(unrepresentable);
        EXPECT_EQ(result.Error().code, TupleSizePlanningError::VARCHAR_LENGTH_TOO_LARGE);
        EXPECT_EQ(result.Error().varlen_index, 0U);

        const std::array ignored_null_length{
            VarlenValueSize{
                .length = std::numeric_limits<std::size_t>::max(),
                .is_null = true,
            },
        };
        const std::optional<TupleSizePlan> expected_null{TupleSizePlan{
            .varlen_payload_size = 0,
            .total_size = layout.MinimumTupleSize(),
        }};
        EXPECT_EQ(layout.PlanTupleSize(ignored_null_length).AsOptional(), expected_null);
    }
}

TEST(TupleSizePlanningTest, AcceptsExactRawLimitAndRejectsOneByteBeyond) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{1});
    if (!built.Ok()) {
        ADD_FAILURE() << "VARCHAR limit layout unexpectedly failed";
        return;
    }
    const auto& layout = *built;
    const std::size_t exact_payload = HEAP_PAGE_MAX_RAW_TUPLE_SIZE - layout.MinimumTupleSize();
    const std::array exact{VarlenValueSize{.length = exact_payload, .is_null = false}};
    const auto exact_result = layout.PlanTupleSize(exact);
    const std::optional<TupleSizePlan> expected_exact{TupleSizePlan{
        .varlen_payload_size = exact_payload,
        .total_size = HEAP_PAGE_MAX_RAW_TUPLE_SIZE,
    }};
    EXPECT_EQ(exact_result.AsOptional(), expected_exact);

    const std::array too_large{VarlenValueSize{.length = exact_payload + 1U, .is_null = false}};
    EXPECT_EQ(layout.PlanTupleSize(too_large).Error().code,
              TupleSizePlanningError::TUPLE_TOO_LARGE);
}

TEST(TuplePhysicalLayoutTest, ComposesNullBitmapSizeAndSchemaVersionWithTupleHeader) {
    constexpr std::array columns{
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = false},
        PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = true},
        PhysicalColumnSpec{.type = PhysicalType::TIMESTAMP, .nullable = false},
    };
    const auto built = BuildTuplePhysicalLayout(columns, SchemaVer{23});
    if (!built.Ok()) {
        ADD_FAILURE() << "TupleHeader composition layout unexpectedly failed";
        return;
    }
    const auto& layout = *built;

    const TupleHeader header{
        .xmin = TxnId{41},
        .xmax = INVALID_TXN_ID,
        .cmin = CommandId{0},
        .cmax = CommandId{0},
        .prev_page_no = INVALID_PAGE_NO,
        .prev_slot = INVALID_SLOT_ID,
        .tuple_flags = TUPLE_FLAG_HAS_NULLS | TUPLE_FLAG_HAS_VARLEN,
        .header_bytes = TUPLE_HEADER_ENCODED_SIZE,
        .null_bitmap_bytes = layout.NullBitmapSize(),
        .schema_version = layout.SchemaVersion(),
        .reserved = 0,
    };
    std::array<std::byte, TUPLE_HEADER_ENCODED_SIZE> encoded{};
    ASSERT_TRUE(EncodeTupleHeader(encoded, header));
    const auto decoded = DecodeTupleHeader(encoded);
    if (!decoded.Ok()) {
        ADD_FAILURE() << "composed TupleHeader unexpectedly failed to decode";
        return;
    }
    const auto& decoded_header = *decoded;
    EXPECT_EQ(decoded_header.null_bitmap_bytes, layout.NullBitmapSize());
    EXPECT_EQ(decoded_header.schema_version, layout.SchemaVersion());
}

} // namespace
} // namespace dblusblus::storage
