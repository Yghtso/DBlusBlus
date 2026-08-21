#ifndef DBLUSBLUS_STORAGE_TUPLE_HEADER_H_
#define DBLUSBLUS_STORAGE_TUPLE_HEADER_H_

#include "common/types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace dblusblus {

using TupleFlags = std::uint16_t;

inline constexpr TupleFlags TUPLE_FLAG_HAS_NULLS = 0x0001U;
inline constexpr TupleFlags TUPLE_FLAG_HAS_VARLEN = 0x0002U;
inline constexpr TupleFlags TUPLE_FLAGS_KNOWN_MASK = TUPLE_FLAG_HAS_NULLS | TUPLE_FLAG_HAS_VARLEN;

inline constexpr std::size_t TUPLE_HEADER_XMIN_OFFSET = 0;
inline constexpr std::size_t TUPLE_HEADER_XMAX_OFFSET = 8;
inline constexpr std::size_t TUPLE_HEADER_CMIN_OFFSET = 16;
inline constexpr std::size_t TUPLE_HEADER_CMAX_OFFSET = 20;
inline constexpr std::size_t TUPLE_HEADER_PREV_PAGE_NO_OFFSET = 24;
inline constexpr std::size_t TUPLE_HEADER_PREV_SLOT_OFFSET = 32;
inline constexpr std::size_t TUPLE_HEADER_FLAGS_OFFSET = 34;
inline constexpr std::size_t TUPLE_HEADER_HEADER_BYTES_OFFSET = 36;
inline constexpr std::size_t TUPLE_HEADER_NULL_BITMAP_BYTES_OFFSET = 38;
inline constexpr std::size_t TUPLE_HEADER_SCHEMA_VERSION_OFFSET = 40;
inline constexpr std::size_t TUPLE_HEADER_RESERVED_OFFSET = 44;
inline constexpr std::size_t TUPLE_HEADER_ENCODED_SIZE = 48;

struct TupleHeader {
    TxnId xmin{INVALID_TXN_ID};
    TxnId xmax{INVALID_TXN_ID};
    CommandId cmin{0};
    CommandId cmax{0};
    PageNo prev_page_no{INVALID_PAGE_NO};
    SlotId prev_slot{INVALID_SLOT_ID};
    TupleFlags tuple_flags{0};
    std::uint16_t header_bytes{TUPLE_HEADER_ENCODED_SIZE};
    std::uint16_t null_bitmap_bytes{0};
    SchemaVer schema_version{0};
    std::uint32_t reserved{0};

    bool operator==(const TupleHeader&) const = default;
};

enum class TupleHeaderDecodeError : std::uint8_t {
    NONE,
    SOURCE_TOO_SMALL,
    INVALID_PREVIOUS_VERSION_POINTER,
    INVALID_FLAGS,
    INVALID_HEADER_SIZE,
    NONZERO_RESERVED,
};

struct TupleHeaderDecodeResult {
    std::optional<TupleHeader> header;
    TupleHeaderDecodeError error{TupleHeaderDecodeError::NONE};

    [[nodiscard]] explicit operator bool() const noexcept {
        return header.has_value();
    }
};

[[nodiscard]] bool EncodeTupleHeader(std::span<std::byte> destination,
                                     const TupleHeader& header) noexcept;
[[nodiscard]] TupleHeaderDecodeResult DecodeTupleHeader(std::span<const std::byte> source) noexcept;

static_assert(sizeof(TxnId) == 8);
static_assert(sizeof(CommandId) == 4);
static_assert(sizeof(PageNo) == 8);
static_assert(sizeof(SlotId) == 2);
static_assert(sizeof(TupleFlags) == 2);
static_assert(sizeof(SchemaVer) == 4);
static_assert(TUPLE_HEADER_RESERVED_OFFSET + sizeof(std::uint32_t) == TUPLE_HEADER_ENCODED_SIZE);

} // namespace dblusblus

#endif // DBLUSBLUS_STORAGE_TUPLE_HEADER_H_
