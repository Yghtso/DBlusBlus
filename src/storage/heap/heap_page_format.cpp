#include "storage/heap/heap_page_format.h"

#include "common/encoding.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>

namespace dblusblus {
namespace {

constexpr std::size_t SLOT_COUNT_OFFSET = 0;
constexpr std::size_t FREE_SLOT_HEAD_OFFSET = 2;
constexpr std::size_t LOWER_OFFSET = 4;
constexpr std::size_t UPPER_OFFSET = 6;
constexpr std::size_t PRUNE_HINT_OFFSET = 8;
constexpr std::size_t RESERVED_OFFSET = 12;

[[nodiscard]] constexpr bool IsValidHeapSlotState(HeapSlotState state) noexcept {
    switch (state) {
    case HeapSlotState::UNUSED:
    case HeapSlotState::NORMAL:
    case HeapSlotState::DEAD:
    case HeapSlotState::REDIRECT_RESERVED:
        return true;
    }
    return false;
}

} // namespace

bool EncodeHeapPageHeader(std::span<std::byte> destination, const HeapPageHeader& header) noexcept {
    if (destination.size() < HEAP_PAGE_HEADER_ENCODED_SIZE) {
        return false;
    }

    const bool slot_count_encoded = EncodeLittleEndian(
        destination.subspan(SLOT_COUNT_OFFSET, sizeof(header.slot_count)), header.slot_count);
    const bool free_slot_head_encoded = EncodeLittleEndian(
        destination.subspan(FREE_SLOT_HEAD_OFFSET, sizeof(header.free_slot_head)),
        header.free_slot_head);
    const bool lower_encoded =
        EncodeLittleEndian(destination.subspan(LOWER_OFFSET, sizeof(header.lower)), header.lower);
    const bool upper_encoded =
        EncodeLittleEndian(destination.subspan(UPPER_OFFSET, sizeof(header.upper)), header.upper);
    const bool prune_hint_encoded = EncodeLittleEndian(
        destination.subspan(PRUNE_HINT_OFFSET, sizeof(header.prune_hint)), header.prune_hint);
    const bool reserved_encoded = EncodeLittleEndian(
        destination.subspan(RESERVED_OFFSET, sizeof(header.reserved)), header.reserved);

    return slot_count_encoded && free_slot_head_encoded && lower_encoded && upper_encoded &&
           prune_hint_encoded && reserved_encoded;
}

std::optional<HeapPageHeader> DecodeHeapPageHeader(std::span<const std::byte> source) noexcept {
    if (source.size() < HEAP_PAGE_HEADER_ENCODED_SIZE) {
        return std::nullopt;
    }

    const auto slot_count =
        DecodeLittleEndian<std::uint16_t>(source.subspan(SLOT_COUNT_OFFSET, sizeof(std::uint16_t)));
    const auto free_slot_head =
        DecodeLittleEndian<SlotId>(source.subspan(FREE_SLOT_HEAD_OFFSET, sizeof(SlotId)));
    const auto lower =
        DecodeLittleEndian<std::uint16_t>(source.subspan(LOWER_OFFSET, sizeof(std::uint16_t)));
    const auto upper =
        DecodeLittleEndian<std::uint16_t>(source.subspan(UPPER_OFFSET, sizeof(std::uint16_t)));
    const auto prune_hint =
        DecodeLittleEndian<std::uint32_t>(source.subspan(PRUNE_HINT_OFFSET, sizeof(std::uint32_t)));
    const auto reserved =
        DecodeLittleEndian<std::uint32_t>(source.subspan(RESERVED_OFFSET, sizeof(std::uint32_t)));
    if (!slot_count.has_value() || !free_slot_head.has_value() || !lower.has_value() ||
        !upper.has_value() || !prune_hint.has_value() || !reserved.has_value()) {
        return std::nullopt;
    }

    return HeapPageHeader{
        .slot_count = *slot_count,
        .free_slot_head = *free_slot_head,
        .lower = *lower,
        .upper = *upper,
        .prune_hint = *prune_hint,
        .reserved = *reserved,
    };
}

bool EncodeHeapSlotEntry(std::span<std::byte> destination, const HeapSlotEntry& entry) noexcept {
    if (destination.size() < HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE ||
        !IsValidHeapSlotState(entry.state)) {
        return false;
    }

    using SlotStateValue = std::underlying_type_t<HeapSlotState>;
    const bool tuple_offset_encoded = EncodeLittleEndian(
        destination.subspan(HEAP_SLOT_TUPLE_OFFSET_OFFSET, sizeof(entry.tuple_offset)),
        entry.tuple_offset);
    const bool tuple_length_encoded = EncodeLittleEndian(
        destination.subspan(HEAP_SLOT_TUPLE_LENGTH_OFFSET, sizeof(entry.tuple_length)),
        entry.tuple_length);
    const bool state_encoded =
        EncodeLittleEndian(destination.subspan(HEAP_SLOT_FLAGS_OFFSET, sizeof(SlotStateValue)),
                           static_cast<SlotStateValue>(entry.state));
    const bool aux_encoded =
        EncodeLittleEndian(destination.subspan(HEAP_SLOT_AUX_OFFSET, sizeof(entry.aux)), entry.aux);

    return tuple_offset_encoded && tuple_length_encoded && state_encoded && aux_encoded;
}

HeapSlotEntryDecodeResult DecodeHeapSlotEntry(std::span<const std::byte> source) noexcept {
    if (source.size() < HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE) {
        return {.entry = std::nullopt, .error = HeapSlotEntryDecodeError::BUFFER_TOO_SMALL};
    }

    using SlotStateValue = std::underlying_type_t<HeapSlotState>;
    const auto tuple_offset = DecodeLittleEndian<std::uint16_t>(
        source.subspan(HEAP_SLOT_TUPLE_OFFSET_OFFSET, sizeof(std::uint16_t)));
    const auto tuple_length = DecodeLittleEndian<std::uint16_t>(
        source.subspan(HEAP_SLOT_TUPLE_LENGTH_OFFSET, sizeof(std::uint16_t)));
    const auto raw_state = DecodeLittleEndian<SlotStateValue>(
        source.subspan(HEAP_SLOT_FLAGS_OFFSET, sizeof(SlotStateValue)));
    const auto aux = DecodeLittleEndian<std::uint16_t>(
        source.subspan(HEAP_SLOT_AUX_OFFSET, sizeof(std::uint16_t)));
    if (!tuple_offset.has_value() || !tuple_length.has_value() || !raw_state.has_value() ||
        !aux.has_value()) {
        return {.entry = std::nullopt, .error = HeapSlotEntryDecodeError::BUFFER_TOO_SMALL};
    }

    const auto state = static_cast<HeapSlotState>(*raw_state);
    if (!IsValidHeapSlotState(state)) {
        return {.entry = std::nullopt, .error = HeapSlotEntryDecodeError::INVALID_SLOT_STATE};
    }

    return HeapSlotEntryDecodeResult{
        .entry =
            HeapSlotEntry{
                .tuple_offset = *tuple_offset,
                .tuple_length = *tuple_length,
                .state = state,
                .aux = *aux,
            },
        .error = HeapSlotEntryDecodeError::NONE,
    };
}

} // namespace dblusblus
