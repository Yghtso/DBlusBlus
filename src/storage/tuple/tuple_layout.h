#pragma once

#include "common/result.h"
#include "common/types.h"
#include "storage/tuple/tuple_header.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace dblusblus {

inline constexpr std::size_t BOOLEAN_PHYSICAL_SIZE = 1;
inline constexpr std::size_t INT32_PHYSICAL_SIZE = 4;
inline constexpr std::size_t INT64_PHYSICAL_SIZE = 8;
inline constexpr std::size_t FLOAT64_PHYSICAL_SIZE = 8;
inline constexpr std::size_t DATE_PHYSICAL_SIZE = 4;
inline constexpr std::size_t TIMESTAMP_PHYSICAL_SIZE = 8;
inline constexpr std::size_t VARCHAR_DESCRIPTOR_OFFSET_OFFSET = 0;
inline constexpr std::size_t VARCHAR_DESCRIPTOR_LENGTH_OFFSET = 4;
inline constexpr std::size_t VARCHAR_DESCRIPTOR_ENCODED_SIZE = 8;

// This is an in-memory storage-layout classification. Its numeric values are not persisted.
enum class PhysicalType : std::uint8_t {
    BOOLEAN,
    INT32,
    INT64,
    FLOAT64,
    DATE,
    TIMESTAMP,
    VARCHAR,
};

[[nodiscard]] std::optional<std::size_t> PhysicalTypeWidth(PhysicalType type) noexcept;
[[nodiscard]] std::optional<std::uint16_t> NullBitmapBytes(std::size_t column_count) noexcept;

enum class NullBitmapError : std::uint8_t {
    COLUMN_COUNT_TOO_LARGE,
    COLUMN_OUT_OF_RANGE,
    BITMAP_TOO_SMALL,
};

using NullBitmapReadResult = Result<bool, NullBitmapError>;

[[nodiscard]] NullBitmapReadResult IsNull(std::span<const std::byte> bitmap,
                                          std::size_t column_count,
                                          std::size_t column_index) noexcept;
[[nodiscard]] Status<NullBitmapError>
SetNull(std::span<std::byte> bitmap, std::size_t column_count, std::size_t column_index) noexcept;
[[nodiscard]] Status<NullBitmapError>
ClearNull(std::span<std::byte> bitmap, std::size_t column_count, std::size_t column_index) noexcept;

struct PhysicalColumnSpec {
    PhysicalType type{PhysicalType::BOOLEAN};
    bool nullable{false};

    bool operator==(const PhysicalColumnSpec&) const = default;
};

struct PhysicalColumnLayout {
    PhysicalType type{PhysicalType::BOOLEAN};
    bool nullable{false};
    std::size_t fixed_offset{0};
    std::size_t fixed_width{0};
    std::size_t null_bit_index{0};

    bool operator==(const PhysicalColumnLayout&) const = default;
};

struct VarlenValueSize {
    std::size_t length{0};
    bool is_null{false};
};

struct TupleSizePlan {
    std::size_t varlen_payload_size{0};
    std::size_t total_size{0};

    bool operator==(const TupleSizePlan&) const = default;
};

enum class TupleSizePlanningError : std::uint8_t {
    VARLEN_VALUE_COUNT_MISMATCH,
    VARCHAR_LENGTH_TOO_LARGE,
    SIZE_OVERFLOW,
    TUPLE_TOO_LARGE,
};

struct TupleSizePlanningFailure {
    TupleSizePlanningError code{TupleSizePlanningError::SIZE_OVERFLOW};
    // The variable-length value the failure concerns, for value-specific codes.
    std::size_t varlen_index{0};

    bool operator==(const TupleSizePlanningFailure&) const = default;
};

using TupleSizePlanningResult = Result<TupleSizePlan, TupleSizePlanningFailure>;

enum class TuplePhysicalLayoutBuildError : std::uint8_t {
    INVALID_PHYSICAL_TYPE,
    NULL_BITMAP_TOO_LARGE,
    SIZE_OVERFLOW,
    TUPLE_TOO_LARGE,
};

struct TuplePhysicalLayoutBuildFailure {
    TuplePhysicalLayoutBuildError code{TuplePhysicalLayoutBuildError::SIZE_OVERFLOW};
    // The column the failure concerns, for column-specific codes.
    std::size_t column_index{0};

    bool operator==(const TuplePhysicalLayoutBuildFailure&) const = default;
};

class TuplePhysicalLayout;
using TuplePhysicalLayoutBuildResult = Result<TuplePhysicalLayout, TuplePhysicalLayoutBuildFailure>;

class TuplePhysicalLayout {
  public:
    [[nodiscard]] SchemaVer SchemaVersion() const noexcept;
    [[nodiscard]] std::size_t ColumnCount() const noexcept;
    [[nodiscard]] std::span<const PhysicalColumnLayout> Columns() const noexcept;
    [[nodiscard]] const PhysicalColumnLayout* Column(std::size_t column_index) const noexcept;
    [[nodiscard]] std::uint16_t NullBitmapSize() const noexcept;
    [[nodiscard]] std::size_t FixedAreaOffset() const noexcept;
    [[nodiscard]] std::size_t FixedAreaSize() const noexcept;
    [[nodiscard]] std::size_t VarlenPayloadOffset() const noexcept;
    [[nodiscard]] std::size_t MinimumTupleSize() const noexcept;
    [[nodiscard]] std::size_t VarlenColumnCount() const noexcept;
    [[nodiscard]] bool HasNullableColumns() const noexcept;
    [[nodiscard]] bool HasVarlenColumns() const noexcept;
    [[nodiscard]] TupleSizePlanningResult
    PlanTupleSize(std::span<const VarlenValueSize> varlen_values) const noexcept;

    bool operator==(const TuplePhysicalLayout&) const = default;

  private:
    SchemaVer schema_version_{0};
    std::vector<PhysicalColumnLayout> columns_;
    std::uint16_t null_bitmap_bytes_{0};
    std::size_t fixed_area_offset_{TUPLE_HEADER_ENCODED_SIZE};
    std::size_t fixed_area_size_{0};
    std::size_t varlen_payload_offset_{TUPLE_HEADER_ENCODED_SIZE};
    std::size_t varlen_column_count_{0};
    bool has_nullable_columns_{false};

    friend TuplePhysicalLayoutBuildResult
    BuildTuplePhysicalLayout(std::span<const PhysicalColumnSpec> columns, SchemaVer schema_version);
};

[[nodiscard]] TuplePhysicalLayoutBuildResult
BuildTuplePhysicalLayout(std::span<const PhysicalColumnSpec> columns, SchemaVer schema_version);

static_assert(VARCHAR_DESCRIPTOR_LENGTH_OFFSET + sizeof(std::uint32_t) ==
              VARCHAR_DESCRIPTOR_ENCODED_SIZE);

} // namespace dblusblus
