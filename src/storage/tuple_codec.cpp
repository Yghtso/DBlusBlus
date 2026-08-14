#include "storage/tuple_codec.h"

#include "common/encoding.h"
#include "storage/heap_page.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace dblusblus {
namespace {

[[nodiscard]] bool IsNullValue(const TupleValue& value) noexcept {
    return std::holds_alternative<std::monostate>(value);
}

[[nodiscard]] bool ValueMatchesType(PhysicalType type, const TupleValue& value) noexcept {
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
        return std::holds_alternative<VarcharValue>(value);
    }
    return false;
}

[[nodiscard]] TupleCodecError TupleErrorFromScalar(FixedScalarCodecError error) noexcept {
    switch (error) {
    case FixedScalarCodecError::NONE:
        return TupleCodecError::NONE;
    case FixedScalarCodecError::TYPE_MISMATCH:
        return TupleCodecError::TYPE_MISMATCH;
    case FixedScalarCodecError::UNSUPPORTED_VARLEN_TYPE:
        return TupleCodecError::MALFORMED_TUPLE;
    case FixedScalarCodecError::INVALID_BOOLEAN:
        return TupleCodecError::INVALID_BOOLEAN;
    case FixedScalarCodecError::DESTINATION_TOO_SMALL:
    case FixedScalarCodecError::SOURCE_TOO_SMALL:
    case FixedScalarCodecError::INVALID_PHYSICAL_TYPE:
        return TupleCodecError::MALFORMED_TUPLE;
    }
    return TupleCodecError::MALFORMED_TUPLE;
}

[[nodiscard]] TupleCodecError TupleErrorFromSizePlanning(TupleSizePlanningError error) noexcept {
    switch (error) {
    case TupleSizePlanningError::NONE:
        return TupleCodecError::NONE;
    case TupleSizePlanningError::VARCHAR_LENGTH_TOO_LARGE:
        return TupleCodecError::VARCHAR_LENGTH_TOO_LARGE;
    case TupleSizePlanningError::SIZE_OVERFLOW:
    case TupleSizePlanningError::TUPLE_TOO_LARGE:
        return TupleCodecError::TUPLE_TOO_LARGE;
    case TupleSizePlanningError::VARLEN_VALUE_COUNT_MISMATCH:
        return TupleCodecError::MALFORMED_TUPLE;
    }
    return TupleCodecError::MALFORMED_TUPLE;
}

[[nodiscard]] std::size_t VarlenSchemaColumnIndex(const TuplePhysicalLayout& layout,
                                                  std::size_t varlen_index) noexcept {
    std::size_t current_varlen_index = 0;
    for (std::size_t column_index = 0; column_index < layout.ColumnCount(); ++column_index) {
        if (layout.Columns()[column_index].type != PhysicalType::VARCHAR) {
            continue;
        }
        if (current_varlen_index == varlen_index) {
            return column_index;
        }
        ++current_varlen_index;
    }
    return layout.ColumnCount();
}

[[nodiscard]] bool CheckedDescriptorEnd(const VarcharDescriptor& descriptor,
                                        std::uint32_t& end) noexcept {
    if (descriptor.payload_length >
        std::numeric_limits<std::uint32_t>::max() - descriptor.payload_offset) {
        return false;
    }
    end = descriptor.payload_offset + descriptor.payload_length;
    return true;
}

[[nodiscard]] std::uint8_t UnusedBitmapBitMask(std::size_t column_count) noexcept {
    const std::size_t used_bits = column_count % 8U;
    if (used_bits == 0) {
        return 0;
    }
    return static_cast<std::uint8_t>(0xFFU << used_bits);
}

} // namespace

VarcharDescriptorCodecError EncodeVarcharDescriptor(std::span<std::byte> destination,
                                                    const VarcharDescriptor& descriptor) noexcept {
    if (destination.size() < VARCHAR_DESCRIPTOR_ENCODED_SIZE) {
        return VarcharDescriptorCodecError::BUFFER_TOO_SMALL;
    }

    std::array<std::byte, VARCHAR_DESCRIPTOR_ENCODED_SIZE> encoded{};
    const auto encoded_bytes = std::span<std::byte>{encoded};
    const bool encoded_all =
        EncodeLittleEndian(encoded_bytes.subspan(VARCHAR_DESCRIPTOR_OFFSET_OFFSET,
                                                 sizeof(descriptor.payload_offset)),
                           descriptor.payload_offset) &&
        EncodeLittleEndian(encoded_bytes.subspan(VARCHAR_DESCRIPTOR_LENGTH_OFFSET,
                                                 sizeof(descriptor.payload_length)),
                           descriptor.payload_length);
    if (!encoded_all) {
        return VarcharDescriptorCodecError::BUFFER_TOO_SMALL;
    }

    std::ranges::copy(encoded, destination.begin());
    return VarcharDescriptorCodecError::NONE;
}

VarcharDescriptorDecodeResult DecodeVarcharDescriptor(std::span<const std::byte> source) noexcept {
    if (source.size() < VARCHAR_DESCRIPTOR_ENCODED_SIZE) {
        return {
            .descriptor = std::nullopt,
            .error = VarcharDescriptorCodecError::BUFFER_TOO_SMALL,
        };
    }

    const auto payload_offset = DecodeLittleEndian<std::uint32_t>(
        source.subspan(VARCHAR_DESCRIPTOR_OFFSET_OFFSET, sizeof(std::uint32_t)));
    const auto payload_length = DecodeLittleEndian<std::uint32_t>(
        source.subspan(VARCHAR_DESCRIPTOR_LENGTH_OFFSET, sizeof(std::uint32_t)));
    if (!payload_offset.has_value() || !payload_length.has_value()) {
        return {
            .descriptor = std::nullopt,
            .error = VarcharDescriptorCodecError::BUFFER_TOO_SMALL,
        };
    }

    return {
        .descriptor =
            VarcharDescriptor{
                .payload_offset = *payload_offset,
                .payload_length = *payload_length,
            },
        .error = VarcharDescriptorCodecError::NONE,
    };
}

FixedScalarCodecError EncodeFixedScalar(std::span<std::byte> destination,
                                        PhysicalType type,
                                        const TupleValue& value) noexcept {
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
            .value = TupleValue{encoded == 1U},
            .error = FixedScalarCodecError::NONE,
        };
    }
    case PhysicalType::INT32: {
        const auto decoded = DecodeLittleEndian<std::int32_t>(source.first(INT32_PHYSICAL_SIZE));
        return {
            .value = decoded.has_value() ? std::optional<TupleValue>{*decoded} : std::nullopt,
            .error = decoded.has_value() ? FixedScalarCodecError::NONE
                                         : FixedScalarCodecError::SOURCE_TOO_SMALL,
        };
    }
    case PhysicalType::INT64: {
        const auto decoded = DecodeLittleEndian<std::int64_t>(source.first(INT64_PHYSICAL_SIZE));
        return {
            .value = decoded.has_value() ? std::optional<TupleValue>{*decoded} : std::nullopt,
            .error = decoded.has_value() ? FixedScalarCodecError::NONE
                                         : FixedScalarCodecError::SOURCE_TOO_SMALL,
        };
    }
    case PhysicalType::FLOAT64: {
        const auto decoded = DecodeLittleEndian<std::uint64_t>(source.first(FLOAT64_PHYSICAL_SIZE));
        return {
            .value = decoded.has_value()
                         ? std::optional<TupleValue>{Float64PhysicalValue{.bits = *decoded}}
                         : std::nullopt,
            .error = decoded.has_value() ? FixedScalarCodecError::NONE
                                         : FixedScalarCodecError::SOURCE_TOO_SMALL,
        };
    }
    case PhysicalType::DATE: {
        const auto decoded = DecodeLittleEndian<std::int32_t>(source.first(DATE_PHYSICAL_SIZE));
        return {
            .value = decoded.has_value()
                         ? std::optional<TupleValue>{DatePhysicalValue{.value = *decoded}}
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
                         ? std::optional<TupleValue>{TimestampPhysicalValue{.value = *decoded}}
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

TupleEncodeResult EncodeTuple(const TuplePhysicalLayout& layout,
                              const TupleVersionMetadata& metadata,
                              std::span<const TupleValue> values) {
    if (values.size() != layout.ColumnCount()) {
        return {
            .tuple = std::nullopt,
            .error = TupleCodecError::COLUMN_COUNT_MISMATCH,
        };
    }
    bool has_nulls = false;
    std::vector<VarlenValueSize> varlen_sizes;
    varlen_sizes.reserve(layout.VarlenColumnCount());
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto* column = layout.Column(index);
        if (column == nullptr) {
            return {
                .tuple = std::nullopt,
                .error = TupleCodecError::MALFORMED_TUPLE,
                .column_index = index,
            };
        }
        if (IsNullValue(values[index])) {
            if (!column->nullable) {
                return {
                    .tuple = std::nullopt,
                    .error = TupleCodecError::NULL_NOT_ALLOWED,
                    .column_index = index,
                };
            }
            has_nulls = true;
            if (column->type == PhysicalType::VARCHAR) {
                varlen_sizes.push_back(VarlenValueSize{.length = 0, .is_null = true});
            }
            continue;
        }
        if (!ValueMatchesType(column->type, values[index])) {
            return {
                .tuple = std::nullopt,
                .error = TupleCodecError::TYPE_MISMATCH,
                .column_index = index,
            };
        }
        if (column->type == PhysicalType::VARCHAR) {
            const auto* varchar = std::get_if<VarcharValue>(&values[index]);
            if (varchar == nullptr) {
                return {
                    .tuple = std::nullopt,
                    .error = TupleCodecError::TYPE_MISMATCH,
                    .column_index = index,
                };
            }
            varlen_sizes.push_back(
                VarlenValueSize{.length = varchar->bytes.size(), .is_null = false});
        }
    }

    const auto size_planning = layout.PlanTupleSize(varlen_sizes);
    if (!size_planning.plan.has_value()) {
        return {
            .tuple = std::nullopt,
            .error = TupleErrorFromSizePlanning(size_planning.error),
            .column_index = VarlenSchemaColumnIndex(layout, size_planning.varlen_index),
        };
    }
    const auto& size_plan = *size_planning.plan;

    const TupleHeader header{
        .xmin = metadata.xmin,
        .xmax = metadata.xmax,
        .cmin = metadata.cmin,
        .cmax = metadata.cmax,
        .prev_page_no = metadata.prev_page_no,
        .prev_slot = metadata.prev_slot,
        .tuple_flags = static_cast<TupleFlags>(
            (has_nulls ? TUPLE_FLAG_HAS_NULLS : TupleFlags{0}) |
            (layout.HasVarlenColumns() ? TUPLE_FLAG_HAS_VARLEN : TupleFlags{0})),
        .header_bytes = TUPLE_HEADER_ENCODED_SIZE,
        .null_bitmap_bytes = layout.NullBitmapSize(),
        .schema_version = layout.SchemaVersion(),
        .reserved = 0,
    };
    std::array<std::byte, TUPLE_HEADER_ENCODED_SIZE> encoded_header{};
    if (!EncodeTupleHeader(encoded_header, header)) {
        return {
            .tuple = std::nullopt,
            .error = TupleCodecError::INVALID_HEADER_METADATA,
        };
    }

    std::vector<std::byte> tuple(size_plan.total_size, std::byte{0});
    std::ranges::copy(encoded_header, tuple.begin());
    auto tuple_bytes = std::span<std::byte>{tuple};
    auto bitmap = tuple_bytes.subspan(TUPLE_HEADER_ENCODED_SIZE, layout.NullBitmapSize());
    std::size_t payload_cursor = layout.VarlenPayloadOffset();

    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto& column = layout.Columns()[index];
        if (IsNullValue(values[index])) {
            const auto bitmap_error = SetNull(bitmap, layout.ColumnCount(), index);
            if (bitmap_error != NullBitmapError::NONE) {
                return {
                    .tuple = std::nullopt,
                    .error = TupleCodecError::MALFORMED_TUPLE,
                    .column_index = index,
                };
            }
            if (column.type == PhysicalType::VARCHAR &&
                EncodeVarcharDescriptor(
                    tuple_bytes.subspan(column.fixed_offset, column.fixed_width),
                    VarcharDescriptor{}) != VarcharDescriptorCodecError::NONE) {
                return {
                    .tuple = std::nullopt,
                    .error = TupleCodecError::MALFORMED_TUPLE,
                    .column_index = index,
                };
            }
            continue;
        }

        if (column.type == PhysicalType::VARCHAR) {
            const auto* varchar = std::get_if<VarcharValue>(&values[index]);
            if (varchar == nullptr || payload_cursor > std::numeric_limits<std::uint32_t>::max() ||
                varchar->bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
                return {
                    .tuple = std::nullopt,
                    .error = TupleCodecError::MALFORMED_TUPLE,
                    .column_index = index,
                };
            }
            const VarcharDescriptor descriptor{
                .payload_offset = static_cast<std::uint32_t>(payload_cursor),
                .payload_length = static_cast<std::uint32_t>(varchar->bytes.size()),
            };
            if (EncodeVarcharDescriptor(
                    tuple_bytes.subspan(column.fixed_offset, column.fixed_width), descriptor) !=
                    VarcharDescriptorCodecError::NONE ||
                payload_cursor > tuple.size() ||
                varchar->bytes.size() > tuple.size() - payload_cursor) {
                return {
                    .tuple = std::nullopt,
                    .error = TupleCodecError::MALFORMED_TUPLE,
                    .column_index = index,
                };
            }
            std::ranges::copy(varchar->bytes, tuple_bytes.subspan(payload_cursor).begin());
            payload_cursor += varchar->bytes.size();
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

    if (payload_cursor != size_plan.total_size) {
        return {.tuple = std::nullopt, .error = TupleCodecError::MALFORMED_TUPLE};
    }

    return {.tuple = std::move(tuple), .error = TupleCodecError::NONE};
}

TupleValidationResult ValidateTuple(const TuplePhysicalLayout& layout,
                                    std::span<const std::byte> tuple) noexcept {
    if (tuple.size() < layout.MinimumTupleSize()) {
        return {.header = std::nullopt, .error = TupleCodecError::MALFORMED_TUPLE};
    }
    if (tuple.size() > HEAP_PAGE_MAX_RAW_TUPLE_SIZE) {
        return {.header = std::nullopt, .error = TupleCodecError::TUPLE_TOO_LARGE};
    }

    const auto decoded_header = DecodeTupleHeader(tuple.first(TUPLE_HEADER_ENCODED_SIZE));
    if (!decoded_header.header.has_value()) {
        return {
            .header = std::nullopt,
            .error = TupleCodecError::MALFORMED_TUPLE,
            .header_error = decoded_header.error,
        };
    }
    const auto& header = *decoded_header.header;
    if (header.schema_version != layout.SchemaVersion()) {
        return {
            .header = std::nullopt,
            .error = TupleCodecError::SCHEMA_VERSION_MISMATCH,
        };
    }
    if (header.null_bitmap_bytes != layout.NullBitmapSize()) {
        return {.header = std::nullopt, .error = TupleCodecError::MALFORMED_TUPLE};
    }
    const bool flag_has_varlen = (header.tuple_flags & TUPLE_FLAG_HAS_VARLEN) != 0;
    if (flag_has_varlen != layout.HasVarlenColumns()) {
        return {.header = std::nullopt, .error = TupleCodecError::VARLEN_FLAG_MISMATCH};
    }

    const auto bitmap = tuple.subspan(TUPLE_HEADER_ENCODED_SIZE, layout.NullBitmapSize());
    if (!bitmap.empty()) {
        const auto unused_mask = UnusedBitmapBitMask(layout.ColumnCount());
        const auto last_byte = std::to_integer<std::uint8_t>(bitmap.back());
        if ((last_byte & unused_mask) != 0) {
            return {.header = std::nullopt, .error = TupleCodecError::MALFORMED_TUPLE};
        }
    }

    bool has_nulls = false;
    for (std::size_t index = 0; index < layout.ColumnCount(); ++index) {
        const auto null_state = IsNull(bitmap, layout.ColumnCount(), index);
        if (!null_state) {
            return {
                .header = std::nullopt,
                .error = TupleCodecError::MALFORMED_TUPLE,
                .column_index = index,
            };
        }
        if (!null_state.is_null) {
            continue;
        }
        if (!layout.Columns()[index].nullable) {
            return {
                .header = std::nullopt,
                .error = TupleCodecError::NULL_NOT_ALLOWED,
                .column_index = index,
            };
        }
        has_nulls = true;
    }

    const bool flag_has_nulls = (header.tuple_flags & TUPLE_FLAG_HAS_NULLS) != 0;
    if (flag_has_nulls != has_nulls) {
        return {.header = std::nullopt, .error = TupleCodecError::FLAG_BITMAP_MISMATCH};
    }

    std::size_t expected_payload_offset = layout.VarlenPayloadOffset();
    for (std::size_t index = 0; index < layout.ColumnCount(); ++index) {
        const auto& column = layout.Columns()[index];
        if (column.type != PhysicalType::VARCHAR) {
            continue;
        }

        const auto decoded_descriptor =
            DecodeVarcharDescriptor(tuple.subspan(column.fixed_offset, column.fixed_width));
        if (!decoded_descriptor.descriptor.has_value()) {
            return {
                .header = std::nullopt,
                .error = TupleCodecError::INVALID_VARLEN_DESCRIPTOR,
                .column_index = index,
            };
        }
        const auto& descriptor = *decoded_descriptor.descriptor;
        const auto null_state = IsNull(bitmap, layout.ColumnCount(), index);
        if (!null_state) {
            return {
                .header = std::nullopt,
                .error = TupleCodecError::MALFORMED_TUPLE,
                .column_index = index,
            };
        }
        if (null_state.is_null) {
            if (descriptor != VarcharDescriptor{}) {
                return {
                    .header = std::nullopt,
                    .error = TupleCodecError::INVALID_VARLEN_DESCRIPTOR,
                    .column_index = index,
                };
            }
            continue;
        }

        std::uint32_t descriptor_end = 0;
        if (descriptor.payload_offset < layout.VarlenPayloadOffset() ||
            !CheckedDescriptorEnd(descriptor, descriptor_end) ||
            descriptor.payload_offset > tuple.size() || descriptor_end > tuple.size()) {
            return {
                .header = std::nullopt,
                .error = TupleCodecError::INVALID_VARLEN_DESCRIPTOR,
                .column_index = index,
            };
        }
        if (descriptor.payload_offset != expected_payload_offset) {
            return {
                .header = std::nullopt,
                .error = TupleCodecError::VARLEN_OFFSET_MISMATCH,
                .column_index = index,
            };
        }
        expected_payload_offset = descriptor_end;
    }

    if (expected_payload_offset != tuple.size()) {
        return {.header = std::nullopt, .error = TupleCodecError::TRAILING_BYTES};
    }

    return {.header = header, .error = TupleCodecError::NONE};
}

TupleDecodeResult DecodeTupleValue(const TuplePhysicalLayout& layout,
                                   std::span<const std::byte> tuple,
                                   std::size_t column_index) noexcept {
    if (column_index >= layout.ColumnCount()) {
        return {.value = std::nullopt, .error = TupleCodecError::COLUMN_OUT_OF_RANGE};
    }

    const auto validation = ValidateTuple(layout, tuple);
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
        return {.value = std::nullopt, .error = TupleCodecError::MALFORMED_TUPLE};
    }
    if (null_state.is_null) {
        return {
            .value = TupleValue{std::monostate{}},
            .error = TupleCodecError::NONE,
        };
    }

    const auto& column = layout.Columns()[column_index];
    if (column.type == PhysicalType::VARCHAR) {
        const auto decoded_descriptor =
            DecodeVarcharDescriptor(tuple.subspan(column.fixed_offset, column.fixed_width));
        if (!decoded_descriptor.descriptor.has_value()) {
            return {
                .value = std::nullopt,
                .error = TupleCodecError::INVALID_VARLEN_DESCRIPTOR,
            };
        }
        const auto& descriptor = *decoded_descriptor.descriptor;
        return {
            .value = TupleValue{VarcharValue{
                .bytes = tuple.subspan(descriptor.payload_offset, descriptor.payload_length)}},
            .error = TupleCodecError::NONE,
        };
    }

    const auto decoded =
        DecodeFixedScalar(column.type, tuple.subspan(column.fixed_offset, column.fixed_width));
    if (!decoded.value.has_value()) {
        return {
            .value = std::nullopt,
            .error = TupleErrorFromScalar(decoded.error),
        };
    }
    return {.value = decoded.value, .error = TupleCodecError::NONE};
}

} // namespace dblusblus
