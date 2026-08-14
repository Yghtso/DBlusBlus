#ifndef DBLUSBLUS_STORAGE_TUPLE_CODEC_H_
#define DBLUSBLUS_STORAGE_TUPLE_CODEC_H_

#include "storage/tuple_layout.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace dblusblus {

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

// Construction/testing representation for storage tuples. This is not the execution engine's
// future row-at-a-time or vectorized value representation. All alternatives are fixed-size and
// std::variant stores them inline.
using FixedTupleValue = std::variant<std::monostate,
                                     bool,
                                     std::int32_t,
                                     std::int64_t,
                                     Float64PhysicalValue,
                                     DatePhysicalValue,
                                     TimestampPhysicalValue>;

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
    std::optional<FixedTupleValue> value;
    FixedScalarCodecError error{FixedScalarCodecError::NONE};

    [[nodiscard]] explicit operator bool() const noexcept {
        return value.has_value();
    }
};

[[nodiscard]] FixedScalarCodecError EncodeFixedScalar(std::span<std::byte> destination,
                                                      PhysicalType type,
                                                      const FixedTupleValue& value) noexcept;
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

enum class FixedTupleCodecError : std::uint8_t {
    NONE,
    COLUMN_COUNT_MISMATCH,
    COLUMN_OUT_OF_RANGE,
    TYPE_MISMATCH,
    NULL_NOT_ALLOWED,
    UNSUPPORTED_VARLEN_TYPE,
    TUPLE_TOO_LARGE,
    INVALID_HEADER_METADATA,
    MALFORMED_TUPLE,
    SCHEMA_VERSION_MISMATCH,
    INVALID_BOOLEAN,
    FLAG_BITMAP_MISMATCH,
};

struct FixedTupleEncodeResult {
    std::optional<std::vector<std::byte>> tuple;
    FixedTupleCodecError error{FixedTupleCodecError::NONE};
    std::size_t column_index{0};

    [[nodiscard]] explicit operator bool() const noexcept {
        return tuple.has_value();
    }
};

struct FixedTupleValidationResult {
    std::optional<TupleHeader> header;
    FixedTupleCodecError error{FixedTupleCodecError::NONE};
    TupleHeaderDecodeError header_error{TupleHeaderDecodeError::NONE};
    std::size_t column_index{0};

    [[nodiscard]] explicit operator bool() const noexcept {
        return header.has_value();
    }
};

struct FixedTupleDecodeResult {
    std::optional<FixedTupleValue> value;
    FixedTupleCodecError error{FixedTupleCodecError::NONE};
    TupleHeaderDecodeError header_error{TupleHeaderDecodeError::NONE};

    [[nodiscard]] explicit operator bool() const noexcept {
        return value.has_value();
    }
};

// This owning-vector encoder is a storage construction primitive. It is intentionally not the
// allocation model for future vectorized execution.
[[nodiscard]] FixedTupleEncodeResult EncodeFixedTuple(const TuplePhysicalLayout& layout,
                                                      const TupleVersionMetadata& metadata,
                                                      std::span<const FixedTupleValue> values);

[[nodiscard]] FixedTupleValidationResult
ValidateFixedTuple(const TuplePhysicalLayout& layout, std::span<const std::byte> tuple) noexcept;

[[nodiscard]] FixedTupleDecodeResult DecodeFixedTupleValue(const TuplePhysicalLayout& layout,
                                                           std::span<const std::byte> tuple,
                                                           std::size_t column_index) noexcept;

static_assert(sizeof(double) == sizeof(std::uint64_t));

} // namespace dblusblus

#endif // DBLUSBLUS_STORAGE_TUPLE_CODEC_H_
