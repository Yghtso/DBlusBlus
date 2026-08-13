#include "storage/tuple_header.h"

#include "common/encoding.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace dblusblus {
namespace {

[[nodiscard]] TupleHeaderDecodeError ValidateTupleHeader(const TupleHeader& header) noexcept {
    const bool page_is_invalid = header.prev_page_no == INVALID_PAGE_NO;
    const bool slot_is_invalid = header.prev_slot == INVALID_SLOT_ID;
    if (page_is_invalid != slot_is_invalid) {
        return TupleHeaderDecodeError::INVALID_PREVIOUS_VERSION_POINTER;
    }
    if ((header.tuple_flags & static_cast<TupleFlags>(~TUPLE_FLAGS_KNOWN_MASK)) != 0) {
        return TupleHeaderDecodeError::INVALID_FLAGS;
    }
    if (header.header_bytes != TUPLE_HEADER_ENCODED_SIZE) {
        return TupleHeaderDecodeError::INVALID_HEADER_SIZE;
    }
    if (header.reserved != 0) {
        return TupleHeaderDecodeError::NONZERO_RESERVED;
    }
    return TupleHeaderDecodeError::NONE;
}

} // namespace

bool EncodeTupleHeader(std::span<std::byte> destination, const TupleHeader& header) noexcept {
    if (destination.size() < TUPLE_HEADER_ENCODED_SIZE ||
        ValidateTupleHeader(header) != TupleHeaderDecodeError::NONE) {
        return false;
    }

    std::array<std::byte, TUPLE_HEADER_ENCODED_SIZE> encoded{};
    const auto encoded_bytes = std::span<std::byte>{encoded};
    const bool encoded_all =
        EncodeLittleEndian(encoded_bytes.subspan(TUPLE_HEADER_XMIN_OFFSET, sizeof(TxnId)),
                           header.xmin) &&
        EncodeLittleEndian(encoded_bytes.subspan(TUPLE_HEADER_XMAX_OFFSET, sizeof(TxnId)),
                           header.xmax) &&
        EncodeLittleEndian(encoded_bytes.subspan(TUPLE_HEADER_CMIN_OFFSET, sizeof(CommandId)),
                           header.cmin) &&
        EncodeLittleEndian(encoded_bytes.subspan(TUPLE_HEADER_CMAX_OFFSET, sizeof(CommandId)),
                           header.cmax) &&
        EncodeLittleEndian(encoded_bytes.subspan(TUPLE_HEADER_PREV_PAGE_NO_OFFSET, sizeof(PageNo)),
                           header.prev_page_no) &&
        EncodeLittleEndian(encoded_bytes.subspan(TUPLE_HEADER_PREV_SLOT_OFFSET, sizeof(SlotId)),
                           header.prev_slot) &&
        EncodeLittleEndian(encoded_bytes.subspan(TUPLE_HEADER_FLAGS_OFFSET, sizeof(TupleFlags)),
                           header.tuple_flags) &&
        EncodeLittleEndian(
            encoded_bytes.subspan(TUPLE_HEADER_HEADER_BYTES_OFFSET, sizeof(std::uint16_t)),
            header.header_bytes) &&
        EncodeLittleEndian(
            encoded_bytes.subspan(TUPLE_HEADER_NULL_BITMAP_BYTES_OFFSET, sizeof(std::uint16_t)),
            header.null_bitmap_bytes) &&
        EncodeLittleEndian(
            encoded_bytes.subspan(TUPLE_HEADER_SCHEMA_VERSION_OFFSET, sizeof(SchemaVer)),
            header.schema_version) &&
        EncodeLittleEndian(
            encoded_bytes.subspan(TUPLE_HEADER_RESERVED_OFFSET, sizeof(std::uint32_t)),
            std::uint32_t{0});
    if (!encoded_all) {
        return false;
    }

    for (std::size_t offset = 0; offset < encoded.size(); ++offset) {
        destination[offset] = encoded[offset];
    }
    return true;
}

TupleHeaderDecodeResult DecodeTupleHeader(std::span<const std::byte> source) noexcept {
    if (source.size() < TUPLE_HEADER_ENCODED_SIZE) {
        return {.header = std::nullopt, .error = TupleHeaderDecodeError::SOURCE_TOO_SMALL};
    }

    const auto xmin =
        DecodeLittleEndian<TxnId>(source.subspan(TUPLE_HEADER_XMIN_OFFSET, sizeof(TxnId)));
    const auto xmax =
        DecodeLittleEndian<TxnId>(source.subspan(TUPLE_HEADER_XMAX_OFFSET, sizeof(TxnId)));
    const auto cmin =
        DecodeLittleEndian<CommandId>(source.subspan(TUPLE_HEADER_CMIN_OFFSET, sizeof(CommandId)));
    const auto cmax =
        DecodeLittleEndian<CommandId>(source.subspan(TUPLE_HEADER_CMAX_OFFSET, sizeof(CommandId)));
    const auto prev_page_no = DecodeLittleEndian<PageNo>(
        source.subspan(TUPLE_HEADER_PREV_PAGE_NO_OFFSET, sizeof(PageNo)));
    const auto prev_slot =
        DecodeLittleEndian<SlotId>(source.subspan(TUPLE_HEADER_PREV_SLOT_OFFSET, sizeof(SlotId)));
    const auto tuple_flags = DecodeLittleEndian<TupleFlags>(
        source.subspan(TUPLE_HEADER_FLAGS_OFFSET, sizeof(TupleFlags)));
    const auto header_bytes = DecodeLittleEndian<std::uint16_t>(
        source.subspan(TUPLE_HEADER_HEADER_BYTES_OFFSET, sizeof(std::uint16_t)));
    const auto null_bitmap_bytes = DecodeLittleEndian<std::uint16_t>(
        source.subspan(TUPLE_HEADER_NULL_BITMAP_BYTES_OFFSET, sizeof(std::uint16_t)));
    const auto schema_version = DecodeLittleEndian<SchemaVer>(
        source.subspan(TUPLE_HEADER_SCHEMA_VERSION_OFFSET, sizeof(SchemaVer)));
    const auto reserved = DecodeLittleEndian<std::uint32_t>(
        source.subspan(TUPLE_HEADER_RESERVED_OFFSET, sizeof(std::uint32_t)));

    if (!xmin.has_value() || !xmax.has_value() || !cmin.has_value() || !cmax.has_value() ||
        !prev_page_no.has_value() || !prev_slot.has_value() || !tuple_flags.has_value() ||
        !header_bytes.has_value() || !null_bitmap_bytes.has_value() ||
        !schema_version.has_value() || !reserved.has_value()) {
        return {.header = std::nullopt, .error = TupleHeaderDecodeError::SOURCE_TOO_SMALL};
    }

    const TupleHeader header{
        .xmin = *xmin,
        .xmax = *xmax,
        .cmin = *cmin,
        .cmax = *cmax,
        .prev_page_no = *prev_page_no,
        .prev_slot = *prev_slot,
        .tuple_flags = *tuple_flags,
        .header_bytes = *header_bytes,
        .null_bitmap_bytes = *null_bitmap_bytes,
        .schema_version = *schema_version,
        .reserved = *reserved,
    };
    const auto validation_error = ValidateTupleHeader(header);
    if (validation_error != TupleHeaderDecodeError::NONE) {
        return {.header = std::nullopt, .error = validation_error};
    }

    return {.header = header, .error = TupleHeaderDecodeError::NONE};
}

} // namespace dblusblus
