#include "storage/tuple/tuple_layout.h"

#include "storage/heap/heap_page_format.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>

namespace dblusblus {
namespace {

[[nodiscard]] bool CheckedAdd(std::size_t left, std::size_t right, std::size_t& result) noexcept {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        return false;
    }
    result = left + right;
    return true;
}

struct BitmapAccess {
    std::size_t bitmap_size;
    std::size_t column_count;
    std::size_t column_index;
};

[[nodiscard]] NullBitmapError ValidateBitmapAccess(BitmapAccess access) noexcept {
    const auto required_bytes = NullBitmapBytes(access.column_count);
    if (!required_bytes.has_value()) {
        return NullBitmapError::COLUMN_COUNT_TOO_LARGE;
    }
    if (access.column_index >= access.column_count) {
        return NullBitmapError::COLUMN_OUT_OF_RANGE;
    }
    if (access.bitmap_size < *required_bytes) {
        return NullBitmapError::BITMAP_TOO_SMALL;
    }
    return NullBitmapError::NONE;
}

[[nodiscard]] constexpr std::uint8_t NullBitMask(std::size_t column_index) noexcept {
    return static_cast<std::uint8_t>(1U << (column_index % 8U));
}

} // namespace

std::optional<std::size_t> PhysicalTypeWidth(PhysicalType type) noexcept {
    switch (type) {
    case PhysicalType::BOOLEAN:
        return BOOLEAN_PHYSICAL_SIZE;
    case PhysicalType::INT32:
        return INT32_PHYSICAL_SIZE;
    case PhysicalType::INT64:
        return INT64_PHYSICAL_SIZE;
    case PhysicalType::FLOAT64:
        return FLOAT64_PHYSICAL_SIZE;
    case PhysicalType::DATE:
        return DATE_PHYSICAL_SIZE;
    case PhysicalType::TIMESTAMP:
        return TIMESTAMP_PHYSICAL_SIZE;
    case PhysicalType::VARCHAR:
        return VARCHAR_DESCRIPTOR_ENCODED_SIZE;
    }
    return std::nullopt;
}

std::optional<std::uint16_t> NullBitmapBytes(std::size_t column_count) noexcept {
    const std::size_t bytes =
        (column_count / 8U) + static_cast<std::size_t>(column_count % 8U != 0);
    if (bytes > std::numeric_limits<std::uint16_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(bytes);
}

NullBitmapReadResult IsNull(std::span<const std::byte> bitmap,
                            std::size_t column_count,
                            std::size_t column_index) noexcept {
    const auto error = ValidateBitmapAccess({
        .bitmap_size = bitmap.size(),
        .column_count = column_count,
        .column_index = column_index,
    });
    if (error != NullBitmapError::NONE) {
        return {.is_null = false, .error = error};
    }

    const auto byte_value = std::to_integer<std::uint8_t>(bitmap[column_index / 8U]);
    return {
        .is_null = (byte_value & NullBitMask(column_index)) != 0,
        .error = NullBitmapError::NONE,
    };
}

NullBitmapError
SetNull(std::span<std::byte> bitmap, std::size_t column_count, std::size_t column_index) noexcept {
    const auto error = ValidateBitmapAccess({
        .bitmap_size = bitmap.size(),
        .column_count = column_count,
        .column_index = column_index,
    });
    if (error != NullBitmapError::NONE) {
        return error;
    }

    const std::size_t byte_index = column_index / 8U;
    const auto byte_value = std::to_integer<std::uint8_t>(bitmap[byte_index]);
    bitmap[byte_index] = static_cast<std::byte>(byte_value | NullBitMask(column_index));
    return NullBitmapError::NONE;
}

NullBitmapError ClearNull(std::span<std::byte> bitmap,
                          std::size_t column_count,
                          std::size_t column_index) noexcept {
    const auto error = ValidateBitmapAccess({
        .bitmap_size = bitmap.size(),
        .column_count = column_count,
        .column_index = column_index,
    });
    if (error != NullBitmapError::NONE) {
        return error;
    }

    const std::size_t byte_index = column_index / 8U;
    const auto byte_value = std::to_integer<std::uint8_t>(bitmap[byte_index]);
    const auto inverse_mask = static_cast<std::uint8_t>(~NullBitMask(column_index));
    bitmap[byte_index] = static_cast<std::byte>(byte_value & inverse_mask);
    return NullBitmapError::NONE;
}

SchemaVer TuplePhysicalLayout::SchemaVersion() const noexcept {
    return schema_version_;
}

std::size_t TuplePhysicalLayout::ColumnCount() const noexcept {
    return columns_.size();
}

std::span<const PhysicalColumnLayout> TuplePhysicalLayout::Columns() const noexcept {
    return columns_;
}

const PhysicalColumnLayout* TuplePhysicalLayout::Column(std::size_t column_index) const noexcept {
    if (column_index >= columns_.size()) {
        return nullptr;
    }
    return &columns_[column_index];
}

std::uint16_t TuplePhysicalLayout::NullBitmapSize() const noexcept {
    return null_bitmap_bytes_;
}

std::size_t TuplePhysicalLayout::FixedAreaOffset() const noexcept {
    return fixed_area_offset_;
}

std::size_t TuplePhysicalLayout::FixedAreaSize() const noexcept {
    return fixed_area_size_;
}

std::size_t TuplePhysicalLayout::VarlenPayloadOffset() const noexcept {
    return varlen_payload_offset_;
}

std::size_t TuplePhysicalLayout::MinimumTupleSize() const noexcept {
    return varlen_payload_offset_;
}

std::size_t TuplePhysicalLayout::VarlenColumnCount() const noexcept {
    return varlen_column_count_;
}

bool TuplePhysicalLayout::HasNullableColumns() const noexcept {
    return has_nullable_columns_;
}

bool TuplePhysicalLayout::HasVarlenColumns() const noexcept {
    return varlen_column_count_ != 0;
}

TupleSizePlanningResult
TuplePhysicalLayout::PlanTupleSize(std::span<const VarlenValueSize> varlen_values) const noexcept {
    if (varlen_values.size() != varlen_column_count_) {
        return {
            .plan = std::nullopt,
            .error = TupleSizePlanningError::VARLEN_VALUE_COUNT_MISMATCH,
        };
    }

    std::size_t payload_size = 0;
    for (std::size_t index = 0; index < varlen_values.size(); ++index) {
        const auto& value = varlen_values[index];
        if (value.is_null) {
            continue;
        }
        if (value.length > std::numeric_limits<std::uint32_t>::max()) {
            return {
                .plan = std::nullopt,
                .error = TupleSizePlanningError::VARCHAR_LENGTH_TOO_LARGE,
                .varlen_index = index,
            };
        }
        if (!CheckedAdd(payload_size, value.length, payload_size)) {
            return {
                .plan = std::nullopt,
                .error = TupleSizePlanningError::SIZE_OVERFLOW,
                .varlen_index = index,
            };
        }
    }

    std::size_t total_size = 0;
    if (!CheckedAdd(varlen_payload_offset_, payload_size, total_size)) {
        return {
            .plan = std::nullopt,
            .error = TupleSizePlanningError::SIZE_OVERFLOW,
        };
    }
    if (total_size > HEAP_PAGE_MAX_RAW_TUPLE_SIZE) {
        return {
            .plan = std::nullopt,
            .error = TupleSizePlanningError::TUPLE_TOO_LARGE,
        };
    }

    return {
        .plan = TupleSizePlan{.varlen_payload_size = payload_size, .total_size = total_size},
        .error = TupleSizePlanningError::NONE,
    };
}

TuplePhysicalLayoutBuildResult BuildTuplePhysicalLayout(std::span<const PhysicalColumnSpec> columns,
                                                        SchemaVer schema_version) {
    const auto bitmap_bytes = NullBitmapBytes(columns.size());
    if (!bitmap_bytes.has_value()) {
        return {
            .layout = std::nullopt,
            .error = TuplePhysicalLayoutBuildError::NULL_BITMAP_TOO_LARGE,
        };
    }

    TuplePhysicalLayout layout;
    layout.schema_version_ = schema_version;
    layout.null_bitmap_bytes_ = *bitmap_bytes;
    if (!CheckedAdd(
            TUPLE_HEADER_ENCODED_SIZE, layout.null_bitmap_bytes_, layout.fixed_area_offset_)) {
        return {
            .layout = std::nullopt,
            .error = TuplePhysicalLayoutBuildError::SIZE_OVERFLOW,
        };
    }
    layout.columns_.reserve(columns.size());

    for (std::size_t index = 0; index < columns.size(); ++index) {
        const auto width = PhysicalTypeWidth(columns[index].type);
        if (!width.has_value()) {
            return {
                .layout = std::nullopt,
                .error = TuplePhysicalLayoutBuildError::INVALID_PHYSICAL_TYPE,
                .column_index = index,
            };
        }

        std::size_t fixed_offset = 0;
        if (!CheckedAdd(layout.fixed_area_offset_, layout.fixed_area_size_, fixed_offset) ||
            !CheckedAdd(layout.fixed_area_size_, *width, layout.fixed_area_size_)) {
            return {
                .layout = std::nullopt,
                .error = TuplePhysicalLayoutBuildError::SIZE_OVERFLOW,
                .column_index = index,
            };
        }

        layout.columns_.push_back(PhysicalColumnLayout{
            .type = columns[index].type,
            .nullable = columns[index].nullable,
            .fixed_offset = fixed_offset,
            .fixed_width = *width,
            .null_bit_index = index,
        });
        layout.has_nullable_columns_ = layout.has_nullable_columns_ || columns[index].nullable;
        if (columns[index].type == PhysicalType::VARCHAR) {
            ++layout.varlen_column_count_;
        }
    }

    if (!CheckedAdd(
            layout.fixed_area_offset_, layout.fixed_area_size_, layout.varlen_payload_offset_)) {
        return {
            .layout = std::nullopt,
            .error = TuplePhysicalLayoutBuildError::SIZE_OVERFLOW,
        };
    }
    if (layout.varlen_payload_offset_ > HEAP_PAGE_MAX_RAW_TUPLE_SIZE) {
        return {
            .layout = std::nullopt,
            .error = TuplePhysicalLayoutBuildError::TUPLE_TOO_LARGE,
        };
    }

    return {
        .layout = std::move(layout),
        .error = TuplePhysicalLayoutBuildError::NONE,
    };
}

} // namespace dblusblus
