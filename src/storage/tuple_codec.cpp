#include "storage/tuple_codec.h"

#include "common/encoding.h"
#include "storage/heap_page.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace dblusblus {
namespace {

[[nodiscard]] bool IsNullValue(const FixedTupleValue& value) noexcept {
    return std::holds_alternative<std::monostate>(value);
}

[[nodiscard]] bool ValueMatchesType(PhysicalType type, const FixedTupleValue& value) noexcept {
    switch (type) {
    case PhysicalType::BOOLEAN:
        return std::holds_alternative<bool>(value);
    case PhysicalType::INT32:
        return std::holds_alternative<std::int32_t>(value);
    case PhysicalType::INT64:
        return std::holds_alternative<std::int64_t>(value);
    case PhysicalType::FLOAT64:
        return std::holds_alternative<Float64PhysicalValue>(value);
    case PhysicalType::DATE:
        return std::holds_alternative<DatePhysicalValue>(value);
    case PhysicalType::TIMESTAMP:
        return std::holds_alternative<TimestampPhysicalValue>(value);
    case PhysicalType::VARCHAR:
        return false;
    }
    return false;
}

[[nodiscard]] FixedTupleCodecError TupleErrorFromScalar(FixedScalarCodecError error) noexcept {
    switch (error) {
    case FixedScalarCodecError::NONE:
        return FixedTupleCodecError::NONE;
    case FixedScalarCodecError::TYPE_MISMATCH:
        return FixedTupleCodecError::TYPE_MISMATCH;
    case FixedScalarCodecError::UNSUPPORTED_VARLEN_TYPE:
        return FixedTupleCodecError::UNSUPPORTED_VARLEN_TYPE;
    case FixedScalarCodecError::INVALID_BOOLEAN:
        return FixedTupleCodecError::INVALID_BOOLEAN;
    case FixedScalarCodecError::DESTINATION_TOO_SMALL:
    case FixedScalarCodecError::SOURCE_TOO_SMALL:
    case FixedScalarCodecError::INVALID_PHYSICAL_TYPE:
        return FixedTupleCodecError::MALFORMED_TUPLE;
    }
    return FixedTupleCodecError::MALFORMED_TUPLE;
}

[[nodiscard]] std::uint8_t UnusedBitmapBitMask(std::size_t column_count) noexcept {
    const std::size_t used_bits = column_count % 8U;
    if (used_bits == 0) {
        return 0;
    }
    return static_cast<std::uint8_t>(0xFFU << used_bits);
}

} // namespace

FixedScalarCodecError EncodeFixedScalar(std::span<std::byte> destination,
                                        PhysicalType type,
                                        const FixedTupleValue& value) noexcept {
    const auto width = PhysicalTypeWidth(type);
    if (!width.has_value()) {
        return FixedScalarCodecError::INVALID_PHYSICAL_TYPE;
    }
    if (type == PhysicalType::VARCHAR) {
        return FixedScalarCodecError::UNSUPPORTED_VARLEN_TYPE;
    }
    if (!ValueMatchesType(type, value)) {
        return FixedScalarCodecError::TYPE_MISMATCH;
    }
    if (destination.size() < *width) {
        return FixedScalarCodecError::DESTINATION_TOO_SMALL;
    }

    switch (type) {
    case PhysicalType::BOOLEAN: {
        const auto* boolean = std::get_if<bool>(&value);
        if (boolean == nullptr) {
            return FixedScalarCodecError::TYPE_MISMATCH;
        }
        destination[0] = *boolean ? std::byte{0x01} : std::byte{0x00};
        return FixedScalarCodecError::NONE;
    }
    case PhysicalType::INT32: {
        const auto* integer = std::get_if<std::int32_t>(&value);
        if (integer == nullptr) {
            return FixedScalarCodecError::TYPE_MISMATCH;
        }
        return EncodeLittleEndian(destination.first(INT32_PHYSICAL_SIZE), *integer)
                   ? FixedScalarCodecError::NONE
                   : FixedScalarCodecError::DESTINATION_TOO_SMALL;
    }
    case PhysicalType::INT64: {
        const auto* integer = std::get_if<std::int64_t>(&value);
        if (integer == nullptr) {
            return FixedScalarCodecError::TYPE_MISMATCH;
        }
        return EncodeLittleEndian(destination.first(INT64_PHYSICAL_SIZE), *integer)
                   ? FixedScalarCodecError::NONE
                   : FixedScalarCodecError::DESTINATION_TOO_SMALL;
    }
    case PhysicalType::FLOAT64: {
        const auto* float_value = std::get_if<Float64PhysicalValue>(&value);
        if (float_value == nullptr) {
            return FixedScalarCodecError::TYPE_MISMATCH;
        }
        return EncodeLittleEndian(destination.first(FLOAT64_PHYSICAL_SIZE), float_value->bits)
                   ? FixedScalarCodecError::NONE
                   : FixedScalarCodecError::DESTINATION_TOO_SMALL;
    }
    case PhysicalType::DATE: {
        const auto* date = std::get_if<DatePhysicalValue>(&value);
        if (date == nullptr) {
            return FixedScalarCodecError::TYPE_MISMATCH;
        }
        return EncodeLittleEndian(destination.first(DATE_PHYSICAL_SIZE), date->value)
                   ? FixedScalarCodecError::NONE
                   : FixedScalarCodecError::DESTINATION_TOO_SMALL;
    }
    case PhysicalType::TIMESTAMP: {
        const auto* timestamp = std::get_if<TimestampPhysicalValue>(&value);
        if (timestamp == nullptr) {
            return FixedScalarCodecError::TYPE_MISMATCH;
        }
        return EncodeLittleEndian(destination.first(TIMESTAMP_PHYSICAL_SIZE), timestamp->value)
                   ? FixedScalarCodecError::NONE
                   : FixedScalarCodecError::DESTINATION_TOO_SMALL;
    }
    case PhysicalType::VARCHAR:
        return FixedScalarCodecError::UNSUPPORTED_VARLEN_TYPE;
    }
    return FixedScalarCodecError::INVALID_PHYSICAL_TYPE;
}

FixedScalarDecodeResult DecodeFixedScalar(PhysicalType type,
                                          std::span<const std::byte> source) noexcept {
    const auto width = PhysicalTypeWidth(type);
    if (!width.has_value()) {
        return {.value = std::nullopt, .error = FixedScalarCodecError::INVALID_PHYSICAL_TYPE};
    }
    if (type == PhysicalType::VARCHAR) {
        return {
            .value = std::nullopt,
            .error = FixedScalarCodecError::UNSUPPORTED_VARLEN_TYPE,
        };
    }
    if (source.size() < *width) {
        return {.value = std::nullopt, .error = FixedScalarCodecError::SOURCE_TOO_SMALL};
    }

    switch (type) {
    case PhysicalType::BOOLEAN: {
        const auto encoded = std::to_integer<std::uint8_t>(source[0]);
        if (encoded > 1U) {
            return {.value = std::nullopt, .error = FixedScalarCodecError::INVALID_BOOLEAN};
        }
        return {
            .value = FixedTupleValue{encoded == 1U},
            .error = FixedScalarCodecError::NONE,
        };
    }
    case PhysicalType::INT32: {
        const auto decoded = DecodeLittleEndian<std::int32_t>(source.first(INT32_PHYSICAL_SIZE));
        return {
            .value = decoded.has_value() ? std::optional<FixedTupleValue>{*decoded} : std::nullopt,
            .error = decoded.has_value() ? FixedScalarCodecError::NONE
                                         : FixedScalarCodecError::SOURCE_TOO_SMALL,
        };
    }
    case PhysicalType::INT64: {
        const auto decoded = DecodeLittleEndian<std::int64_t>(source.first(INT64_PHYSICAL_SIZE));
        return {
            .value = decoded.has_value() ? std::optional<FixedTupleValue>{*decoded} : std::nullopt,
            .error = decoded.has_value() ? FixedScalarCodecError::NONE
                                         : FixedScalarCodecError::SOURCE_TOO_SMALL,
        };
    }
    case PhysicalType::FLOAT64: {
        const auto decoded = DecodeLittleEndian<std::uint64_t>(source.first(FLOAT64_PHYSICAL_SIZE));
        return {
            .value = decoded.has_value()
                         ? std::optional<FixedTupleValue>{Float64PhysicalValue{.bits = *decoded}}
                         : std::nullopt,
            .error = decoded.has_value() ? FixedScalarCodecError::NONE
                                         : FixedScalarCodecError::SOURCE_TOO_SMALL,
        };
    }
    case PhysicalType::DATE: {
        const auto decoded = DecodeLittleEndian<std::int32_t>(source.first(DATE_PHYSICAL_SIZE));
        return {
            .value = decoded.has_value()
                         ? std::optional<FixedTupleValue>{DatePhysicalValue{.value = *decoded}}
                         : std::nullopt,
            .error = decoded.has_value() ? FixedScalarCodecError::NONE
                                         : FixedScalarCodecError::SOURCE_TOO_SMALL,
        };
    }
    case PhysicalType::TIMESTAMP: {
        const auto decoded =
            DecodeLittleEndian<std::int64_t>(source.first(TIMESTAMP_PHYSICAL_SIZE));
        return {
            .value = decoded.has_value()
                         ? std::optional<FixedTupleValue>{TimestampPhysicalValue{.value = *decoded}}
                         : std::nullopt,
            .error = decoded.has_value() ? FixedScalarCodecError::NONE
                                         : FixedScalarCodecError::SOURCE_TOO_SMALL,
        };
    }
    case PhysicalType::VARCHAR:
        return {
            .value = std::nullopt,
            .error = FixedScalarCodecError::UNSUPPORTED_VARLEN_TYPE,
        };
    }
    return {.value = std::nullopt, .error = FixedScalarCodecError::INVALID_PHYSICAL_TYPE};
}

FixedTupleEncodeResult EncodeFixedTuple(const TuplePhysicalLayout& layout,
                                        const TupleVersionMetadata& metadata,
                                        std::span<const FixedTupleValue> values) {
    if (values.size() != layout.ColumnCount()) {
        return {
            .tuple = std::nullopt,
            .error = FixedTupleCodecError::COLUMN_COUNT_MISMATCH,
        };
    }
    if (layout.MinimumTupleSize() > HEAP_PAGE_MAX_RAW_TUPLE_SIZE) {
        return {.tuple = std::nullopt, .error = FixedTupleCodecError::TUPLE_TOO_LARGE};
    }

    bool has_nulls = false;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto* column = layout.Column(index);
        if (column == nullptr) {
            return {
                .tuple = std::nullopt,
                .error = FixedTupleCodecError::MALFORMED_TUPLE,
                .column_index = index,
            };
        }
        if (column->type == PhysicalType::VARCHAR) {
            return {
                .tuple = std::nullopt,
                .error = FixedTupleCodecError::UNSUPPORTED_VARLEN_TYPE,
                .column_index = index,
            };
        }
        if (IsNullValue(values[index])) {
            if (!column->nullable) {
                return {
                    .tuple = std::nullopt,
                    .error = FixedTupleCodecError::NULL_NOT_ALLOWED,
                    .column_index = index,
                };
            }
            has_nulls = true;
            continue;
        }
        if (!ValueMatchesType(column->type, values[index])) {
            return {
                .tuple = std::nullopt,
                .error = FixedTupleCodecError::TYPE_MISMATCH,
                .column_index = index,
            };
        }
    }

    const TupleHeader header{
        .xmin = metadata.xmin,
        .xmax = metadata.xmax,
        .cmin = metadata.cmin,
        .cmax = metadata.cmax,
        .prev_page_no = metadata.prev_page_no,
        .prev_slot = metadata.prev_slot,
        .tuple_flags = has_nulls ? TUPLE_FLAG_HAS_NULLS : TupleFlags{0},
        .header_bytes = TUPLE_HEADER_ENCODED_SIZE,
        .null_bitmap_bytes = layout.NullBitmapSize(),
        .schema_version = layout.SchemaVersion(),
        .reserved = 0,
    };
    std::array<std::byte, TUPLE_HEADER_ENCODED_SIZE> encoded_header{};
    if (!EncodeTupleHeader(encoded_header, header)) {
        return {
            .tuple = std::nullopt,
            .error = FixedTupleCodecError::INVALID_HEADER_METADATA,
        };
    }

    std::vector<std::byte> tuple(layout.MinimumTupleSize(), std::byte{0});
    std::ranges::copy(encoded_header, tuple.begin());
    auto tuple_bytes = std::span<std::byte>{tuple};
    auto bitmap = tuple_bytes.subspan(TUPLE_HEADER_ENCODED_SIZE, layout.NullBitmapSize());

    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto& column = layout.Columns()[index];
        if (IsNullValue(values[index])) {
            const auto bitmap_error = SetNull(bitmap, layout.ColumnCount(), index);
            if (bitmap_error != NullBitmapError::NONE) {
                return {
                    .tuple = std::nullopt,
                    .error = FixedTupleCodecError::MALFORMED_TUPLE,
                    .column_index = index,
                };
            }
            continue;
        }

        const auto scalar_error =
            EncodeFixedScalar(tuple_bytes.subspan(column.fixed_offset, column.fixed_width),
                              column.type,
                              values[index]);
        if (scalar_error != FixedScalarCodecError::NONE) {
            return {
                .tuple = std::nullopt,
                .error = TupleErrorFromScalar(scalar_error),
                .column_index = index,
            };
        }
    }

    return {.tuple = std::move(tuple), .error = FixedTupleCodecError::NONE};
}

FixedTupleValidationResult ValidateFixedTuple(const TuplePhysicalLayout& layout,
                                              std::span<const std::byte> tuple) noexcept {
    if (layout.HasVarlenColumns()) {
        return {
            .header = std::nullopt,
            .error = FixedTupleCodecError::UNSUPPORTED_VARLEN_TYPE,
        };
    }
    if (tuple.size() < layout.MinimumTupleSize()) {
        return {.header = std::nullopt, .error = FixedTupleCodecError::MALFORMED_TUPLE};
    }

    const auto decoded_header = DecodeTupleHeader(tuple.first(TUPLE_HEADER_ENCODED_SIZE));
    if (!decoded_header.header.has_value()) {
        return {
            .header = std::nullopt,
            .error = FixedTupleCodecError::MALFORMED_TUPLE,
            .header_error = decoded_header.error,
        };
    }
    const auto& header = *decoded_header.header;
    if (header.schema_version != layout.SchemaVersion()) {
        return {
            .header = std::nullopt,
            .error = FixedTupleCodecError::SCHEMA_VERSION_MISMATCH,
        };
    }
    if (header.null_bitmap_bytes != layout.NullBitmapSize() ||
        (header.tuple_flags & TUPLE_FLAG_HAS_VARLEN) != 0) {
        return {.header = std::nullopt, .error = FixedTupleCodecError::MALFORMED_TUPLE};
    }

    const auto bitmap = tuple.subspan(TUPLE_HEADER_ENCODED_SIZE, layout.NullBitmapSize());
    if (!bitmap.empty()) {
        const auto unused_mask = UnusedBitmapBitMask(layout.ColumnCount());
        const auto last_byte = std::to_integer<std::uint8_t>(bitmap.back());
        if ((last_byte & unused_mask) != 0) {
            return {.header = std::nullopt, .error = FixedTupleCodecError::MALFORMED_TUPLE};
        }
    }

    bool has_nulls = false;
    for (std::size_t index = 0; index < layout.ColumnCount(); ++index) {
        const auto null_state = IsNull(bitmap, layout.ColumnCount(), index);
        if (!null_state) {
            return {
                .header = std::nullopt,
                .error = FixedTupleCodecError::MALFORMED_TUPLE,
                .column_index = index,
            };
        }
        if (!null_state.is_null) {
            continue;
        }
        if (!layout.Columns()[index].nullable) {
            return {
                .header = std::nullopt,
                .error = FixedTupleCodecError::NULL_NOT_ALLOWED,
                .column_index = index,
            };
        }
        has_nulls = true;
    }

    const bool flag_has_nulls = (header.tuple_flags & TUPLE_FLAG_HAS_NULLS) != 0;
    if (flag_has_nulls != has_nulls) {
        return {.header = std::nullopt, .error = FixedTupleCodecError::FLAG_BITMAP_MISMATCH};
    }

    return {.header = header, .error = FixedTupleCodecError::NONE};
}

FixedTupleDecodeResult DecodeFixedTupleValue(const TuplePhysicalLayout& layout,
                                             std::span<const std::byte> tuple,
                                             std::size_t column_index) noexcept {
    if (column_index >= layout.ColumnCount()) {
        return {.value = std::nullopt, .error = FixedTupleCodecError::COLUMN_OUT_OF_RANGE};
    }

    const auto validation = ValidateFixedTuple(layout, tuple);
    if (!validation.header.has_value()) {
        return {
            .value = std::nullopt,
            .error = validation.error,
            .header_error = validation.header_error,
        };
    }

    const auto bitmap = tuple.subspan(TUPLE_HEADER_ENCODED_SIZE, layout.NullBitmapSize());
    const auto null_state = IsNull(bitmap, layout.ColumnCount(), column_index);
    if (!null_state) {
        return {.value = std::nullopt, .error = FixedTupleCodecError::MALFORMED_TUPLE};
    }
    if (null_state.is_null) {
        return {
            .value = FixedTupleValue{std::monostate{}},
            .error = FixedTupleCodecError::NONE,
        };
    }

    const auto& column = layout.Columns()[column_index];
    const auto decoded =
        DecodeFixedScalar(column.type, tuple.subspan(column.fixed_offset, column.fixed_width));
    if (!decoded.value.has_value()) {
        return {
            .value = std::nullopt,
            .error = TupleErrorFromScalar(decoded.error),
        };
    }
    return {.value = decoded.value, .error = FixedTupleCodecError::NONE};
}

} // namespace dblusblus
