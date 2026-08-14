#ifndef DBLUSBLUS_STORAGE_TUPLE_CODEC_H_
#define DBLUSBLUS_STORAGE_TUPLE_CODEC_H_

#include "storage/tuple_layout.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace dblusblus {

struct VarcharDescriptor {
    std::uint32_t payload_offset{0};
    std::uint32_t payload_length{0};

    bool operator==(const VarcharDescriptor&) const = default;
};

enum class VarcharDescriptorCodecError : std::uint8_t {
    NONE,
    BUFFER_TOO_SMALL,
};

struct VarcharDescriptorDecodeResult {
    std::optional<VarcharDescriptor> descriptor;
    VarcharDescriptorCodecError error{VarcharDescriptorCodecError::NONE};

    [[nodiscard]] explicit operator bool() const noexcept {
        return descriptor.has_value();
    }
};

[[nodiscard]] VarcharDescriptorCodecError
EncodeVarcharDescriptor(std::span<std::byte> destination,
                        const VarcharDescriptor& descriptor) noexcept;
[[nodiscard]] VarcharDescriptorDecodeResult
DecodeVarcharDescriptor(std::span<const std::byte> source) noexcept;

struct Float64PhysicalValue {
    std::uint64_t bits{0};

    [[nodiscard]] static constexpr Float64PhysicalValue FromDouble(double value) noexcept {
        return {.bits = std::bit_cast<std::uint64_t>(value)};
    }

    [[nodiscard]] constexpr double ToDouble() const noexcept {
        return std::bit_cast<double>(bits);
    }

    bool operator==(const Float64PhysicalValue&) const = default;
};

struct DatePhysicalValue {
    std::int32_t value{0};

    bool operator==(const DatePhysicalValue&) const = default;
};

struct TimestampPhysicalValue {
    std::int64_t value{0};

    bool operator==(const TimestampPhysicalValue&) const = default;
};

struct VarcharValue {
    std::span<const std::byte> bytes;

    [[nodiscard]] bool operator==(const VarcharValue& other) const noexcept {
        return std::ranges::equal(bytes, other.bytes);
    }
};

// Construction/testing and storage-decode representation. VarcharValue is a non-owning input view
// during encode and a non-owning tuple-backed view after decode. This is not the future execution
// engine's row-at-a-time or vectorized value representation.
using TupleValue = std::variant<std::monostate,
                                bool,
                                std::int32_t,
                                std::int64_t,
                                Float64PhysicalValue,
                                DatePhysicalValue,
                                TimestampPhysicalValue,
                                VarcharValue>;

enum class FixedScalarCodecError : std::uint8_t {
    NONE,
    DESTINATION_TOO_SMALL,
    SOURCE_TOO_SMALL,
    TYPE_MISMATCH,
    UNSUPPORTED_VARLEN_TYPE,
    INVALID_PHYSICAL_TYPE,
    INVALID_BOOLEAN,
};

struct FixedScalarDecodeResult {
    std::optional<TupleValue> value;
    FixedScalarCodecError error{FixedScalarCodecError::NONE};

    [[nodiscard]] explicit operator bool() const noexcept {
        return value.has_value();
    }
};

[[nodiscard]] FixedScalarCodecError EncodeFixedScalar(std::span<std::byte> destination,
                                                      PhysicalType type,
                                                      const TupleValue& value) noexcept;
[[nodiscard]] FixedScalarDecodeResult DecodeFixedScalar(PhysicalType type,
                                                        std::span<const std::byte> source) noexcept;

struct TupleVersionMetadata {
    TxnId xmin{INVALID_TXN_ID};
    TxnId xmax{INVALID_TXN_ID};
    CommandId cmin{0};
    CommandId cmax{0};
    PageNo prev_page_no{INVALID_PAGE_NO};
    SlotId prev_slot{INVALID_SLOT_ID};

    bool operator==(const TupleVersionMetadata&) const = default;
};

enum class TupleCodecError : std::uint8_t {
    NONE,
    COLUMN_COUNT_MISMATCH,
    COLUMN_OUT_OF_RANGE,
    TYPE_MISMATCH,
    NULL_NOT_ALLOWED,
    VARCHAR_LENGTH_TOO_LARGE,
    TUPLE_TOO_LARGE,
    INVALID_HEADER_METADATA,
    MALFORMED_TUPLE,
    SCHEMA_VERSION_MISMATCH,
    INVALID_BOOLEAN,
    FLAG_BITMAP_MISMATCH,
    VARLEN_FLAG_MISMATCH,
    INVALID_VARLEN_DESCRIPTOR,
    VARLEN_OFFSET_MISMATCH,
    TRAILING_BYTES,
};

struct TupleEncodeResult {
    std::optional<std::vector<std::byte>> tuple;
    TupleCodecError error{TupleCodecError::NONE};
    std::size_t column_index{0};

    [[nodiscard]] explicit operator bool() const noexcept {
        return tuple.has_value();
    }
};

struct TupleValidationResult {
    std::optional<TupleHeader> header;
    TupleCodecError error{TupleCodecError::NONE};
    TupleHeaderDecodeError header_error{TupleHeaderDecodeError::NONE};
    std::size_t column_index{0};

    [[nodiscard]] explicit operator bool() const noexcept {
        return header.has_value();
    }
};

struct TupleDecodeResult {
    std::optional<TupleValue> value;
    TupleCodecError error{TupleCodecError::NONE};
    TupleHeaderDecodeError header_error{TupleHeaderDecodeError::NONE};

    [[nodiscard]] explicit operator bool() const noexcept {
        return value.has_value();
    }
};

// This owning-vector encoder is a storage construction primitive. It is intentionally not the
// allocation model for future vectorized execution.
[[nodiscard]] TupleEncodeResult EncodeTuple(const TuplePhysicalLayout& layout,
                                            const TupleVersionMetadata& metadata,
                                            std::span<const TupleValue> values);

[[nodiscard]] TupleValidationResult ValidateTuple(const TuplePhysicalLayout& layout,
                                                  std::span<const std::byte> tuple) noexcept;

// A decoded VarcharValue borrows from tuple and must not outlive that span's backing storage.
[[nodiscard]] TupleDecodeResult DecodeTupleValue(const TuplePhysicalLayout& layout,
                                                 std::span<const std::byte> tuple,
                                                 std::size_t column_index) noexcept;

static_assert(sizeof(double) == sizeof(std::uint64_t));
static_assert(std::numeric_limits<double>::is_iec559);

} // namespace dblusblus

#endif // DBLUSBLUS_STORAGE_TUPLE_CODEC_H_
