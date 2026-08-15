#include "storage/heap_page.h"

#include "common/encoding.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
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
constexpr std::size_t MAX_HEAP_PAGE_SLOT_COUNT =
    (PAGE_SIZE - HEAP_PAGE_SLOT_DIRECTORY_OFFSET) / HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE;

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

[[nodiscard]] HeapPageValidationResult
ValidationFailure(HeapPageValidationError error,
                  std::optional<CommonPageHeader> common_header,
                  std::optional<HeapPageHeader> heap_header = std::nullopt,
                  SlotId slot_id = INVALID_SLOT_ID) noexcept {
    return HeapPageValidationResult{
        .common_header = common_header,
        .heap_header = heap_header,
        .error = error,
        .slot_id = slot_id,
    };
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

HeapPage::HeapPage(Page& page) noexcept : page_(&page) {}

bool HeapPage::Initialize(Lsn page_lsn) noexcept {
    const CommonPageHeader common_header{
        .page_type = PageType::HEAP_DATA,
        .format_version = HEAP_PAGE_FORMAT_VERSION,
        .flags = 0,
        .page_lsn = page_lsn,
        .checksum_crc32c = 0,
        .header_size = HEAP_PAGE_TOTAL_HEADER_SIZE,
        .reserved16 = 0,
        .page_no = page_->Id().page_no,
    };
    if (!page_->Initialize(common_header)) {
        return false;
    }

    return EncodeHeapPageHeader(
        page_->Bytes().subspan(HEAP_PAGE_HEADER_OFFSET, HEAP_PAGE_HEADER_ENCODED_SIZE),
        HeapPageHeader{});
}

std::optional<HeapPageHeader> HeapPage::Header() const noexcept {
    return DecodeHeapPageHeader(
        page_->Bytes().subspan(HEAP_PAGE_HEADER_OFFSET, HEAP_PAGE_HEADER_ENCODED_SIZE));
}

HeapPageValidationResult HeapPage::Validate() const noexcept {
    const auto common_header = page_->DecodeHeader();
    if (!common_header.has_value()) {
        return ValidationFailure(HeapPageValidationError::COMMON_HEADER_DECODE_FAILED,
                                 std::nullopt);
    }
    if (common_header->page_type != PageType::HEAP_DATA) {
        return ValidationFailure(HeapPageValidationError::WRONG_PAGE_TYPE, common_header);
    }
    if (common_header->page_no != page_->Id().page_no) {
        return ValidationFailure(HeapPageValidationError::WRONG_PAGE_NUMBER, common_header);
    }
    if (common_header->header_size != HEAP_PAGE_TOTAL_HEADER_SIZE) {
        return ValidationFailure(HeapPageValidationError::WRONG_HEADER_SIZE, common_header);
    }
    if (common_header->format_version != HEAP_PAGE_FORMAT_VERSION) {
        return ValidationFailure(HeapPageValidationError::UNSUPPORTED_FORMAT_VERSION,
                                 common_header);
    }
    if (common_header->flags != 0) {
        return ValidationFailure(HeapPageValidationError::NONZERO_COMMON_FLAGS, common_header);
    }
    if (common_header->reserved16 != 0) {
        return ValidationFailure(HeapPageValidationError::NONZERO_COMMON_RESERVED, common_header);
    }

    const auto heap_header = Header();
    if (!heap_header.has_value()) {
        return ValidationFailure(HeapPageValidationError::HEAP_HEADER_DECODE_FAILED, common_header);
    }
    if (heap_header->reserved != 0) {
        return ValidationFailure(
            HeapPageValidationError::NONZERO_HEAP_RESERVED, common_header, heap_header);
    }
    if (heap_header->lower < HEAP_PAGE_TOTAL_HEADER_SIZE) {
        return ValidationFailure(
            HeapPageValidationError::LOWER_BEFORE_HEADER, common_header, heap_header);
    }
    if (heap_header->upper > PAGE_SIZE) {
        return ValidationFailure(
            HeapPageValidationError::UPPER_AFTER_PAGE, common_header, heap_header);
    }
    if (heap_header->lower > heap_header->upper) {
        return ValidationFailure(
            HeapPageValidationError::LOWER_AFTER_UPPER, common_header, heap_header);
    }

    const std::size_t slot_directory_size =
        static_cast<std::size_t>(heap_header->slot_count) * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE;
    if (slot_directory_size > PAGE_SIZE - HEAP_PAGE_SLOT_DIRECTORY_OFFSET) {
        return ValidationFailure(
            HeapPageValidationError::SLOT_DIRECTORY_OUT_OF_BOUNDS, common_header, heap_header);
    }
    const std::size_t expected_lower = HEAP_PAGE_SLOT_DIRECTORY_OFFSET + slot_directory_size;
    if (heap_header->lower != expected_lower) {
        return ValidationFailure(
            HeapPageValidationError::SLOT_COUNT_LOWER_MISMATCH, common_header, heap_header);
    }
    if (heap_header->slot_count == 0 && heap_header->free_slot_head != INVALID_SLOT_ID) {
        return ValidationFailure(
            HeapPageValidationError::INVALID_EMPTY_FREE_SLOT_HEAD, common_header, heap_header);
    }

    for (std::uint32_t index = 0; index < heap_header->slot_count; ++index) {
        const std::size_t slot_offset =
            HEAP_PAGE_SLOT_DIRECTORY_OFFSET + (index * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE);
        const auto decoded_slot = DecodeHeapSlotEntry(
            page_->Bytes().subspan(slot_offset, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE));
        const auto slot_id = static_cast<SlotId>(index);
        if (decoded_slot.error == HeapSlotEntryDecodeError::INVALID_SLOT_STATE) {
            return ValidationFailure(
                HeapPageValidationError::INVALID_SLOT_STATE, common_header, heap_header, slot_id);
        }
        if (!decoded_slot.entry.has_value()) {
            return ValidationFailure(HeapPageValidationError::SLOT_DIRECTORY_OUT_OF_BOUNDS,
                                     common_header,
                                     heap_header,
                                     slot_id);
        }

        const HeapSlotEntry& slot = *decoded_slot.entry;
        if (slot.state == HeapSlotState::NORMAL) {
            const std::size_t tuple_offset = slot.tuple_offset;
            const std::size_t tuple_length = slot.tuple_length;
            if (tuple_offset < heap_header->upper || tuple_offset > PAGE_SIZE ||
                tuple_length > PAGE_SIZE - tuple_offset) {
                return ValidationFailure(HeapPageValidationError::NORMAL_TUPLE_OUT_OF_BOUNDS,
                                         common_header,
                                         heap_header,
                                         slot_id);
            }
        }
    }

    return HeapPageValidationResult{
        .common_header = common_header,
        .heap_header = heap_header,
        .error = HeapPageValidationError::NONE,
        .slot_id = INVALID_SLOT_ID,
    };
}

HeapPageInsertResult HeapPage::Insert(std::span<const std::byte> tuple) noexcept {
    const auto validation = Validate();
    if (!validation || !validation.heap_header.has_value()) {
        return HeapPageInsertResult{
            .rid = std::nullopt,
            .error = HeapPageInsertError::PAGE_INVALID,
            .page_error = validation.error,
        };
    }
    if (tuple.size() > std::numeric_limits<std::uint16_t>::max() ||
        tuple.size() > HEAP_PAGE_MAX_RAW_TUPLE_SIZE) {
        return HeapPageInsertResult{
            .rid = std::nullopt,
            .error = HeapPageInsertError::TUPLE_TOO_LARGE,
        };
    }

    HeapPageHeader updated_header = *validation.heap_header;
    if (updated_header.slot_count == std::numeric_limits<std::uint16_t>::max()) {
        return HeapPageInsertResult{
            .rid = std::nullopt,
            .error = HeapPageInsertError::SLOT_ID_EXHAUSTED,
        };
    }

    const std::size_t available_space =
        static_cast<std::size_t>(updated_header.upper) - updated_header.lower;
    const std::size_t required_space = tuple.size() + HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE;
    if (required_space > available_space) {
        return HeapPageInsertResult{
            .rid = std::nullopt,
            .error = HeapPageInsertError::INSUFFICIENT_SPACE,
        };
    }

    const SlotId slot_id = updated_header.slot_count;
    const std::size_t slot_offset = updated_header.lower;
    const std::size_t new_lower = slot_offset + HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE;
    const std::size_t new_upper = static_cast<std::size_t>(updated_header.upper) - tuple.size();
    if (new_lower > std::numeric_limits<std::uint16_t>::max() ||
        new_upper > std::numeric_limits<std::uint16_t>::max()) {
        return HeapPageInsertResult{
            .rid = std::nullopt,
            .error = HeapPageInsertError::PAGE_INVALID,
            .page_error = HeapPageValidationError::SLOT_DIRECTORY_OUT_OF_BOUNDS,
        };
    }

    const HeapSlotEntry slot{
        .tuple_offset = static_cast<std::uint16_t>(new_upper),
        .tuple_length = static_cast<std::uint16_t>(tuple.size()),
        .state = HeapSlotState::NORMAL,
        .aux = 0,
    };
    ++updated_header.slot_count;
    updated_header.lower = static_cast<std::uint16_t>(new_lower);
    updated_header.upper = static_cast<std::uint16_t>(new_upper);

    std::array<std::byte, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE> encoded_slot{};
    std::array<std::byte, HEAP_PAGE_HEADER_ENCODED_SIZE> encoded_header{};
    if (!EncodeHeapSlotEntry(encoded_slot, slot) ||
        !EncodeHeapPageHeader(encoded_header, updated_header)) {
        return HeapPageInsertResult{
            .rid = std::nullopt,
            .error = HeapPageInsertError::PAGE_INVALID,
        };
    }

    auto page_bytes = page_->Bytes();
    if (!tuple.empty()) {
        std::memmove(page_bytes.data() + new_upper, tuple.data(), tuple.size());
    }
    std::ranges::copy(encoded_slot, page_bytes.begin() + static_cast<std::ptrdiff_t>(slot_offset));
    std::ranges::copy(encoded_header,
                      page_bytes.begin() + static_cast<std::ptrdiff_t>(HEAP_PAGE_HEADER_OFFSET));

    return HeapPageInsertResult{
        .rid = Rid{.page = page_->Id(), .slot = slot_id},
        .error = HeapPageInsertError::NONE,
    };
}

HeapPageMarkDeadResult HeapPage::MarkDead(SlotId slot_id) noexcept {
    const auto validation = Validate();
    if (!validation || !validation.heap_header.has_value()) {
        return HeapPageMarkDeadResult{
            .error = HeapPageMarkDeadError::PAGE_INVALID,
            .page_error = validation.error,
        };
    }
    if (slot_id >= validation.heap_header->slot_count) {
        return HeapPageMarkDeadResult{
            .error = HeapPageMarkDeadError::SLOT_OUT_OF_RANGE,
        };
    }

    const std::size_t slot_offset =
        HEAP_PAGE_SLOT_DIRECTORY_OFFSET +
        (static_cast<std::size_t>(slot_id) * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE);
    const auto decoded_slot =
        DecodeHeapSlotEntry(page_->Bytes().subspan(slot_offset, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE));
    if (!decoded_slot.entry.has_value()) {
        return HeapPageMarkDeadResult{
            .error = HeapPageMarkDeadError::PAGE_INVALID,
            .page_error = decoded_slot.error == HeapSlotEntryDecodeError::INVALID_SLOT_STATE
                              ? HeapPageValidationError::INVALID_SLOT_STATE
                              : HeapPageValidationError::SLOT_DIRECTORY_OUT_OF_BOUNDS,
        };
    }

    HeapSlotEntry updated_slot = *decoded_slot.entry;
    switch (updated_slot.state) {
    case HeapSlotState::NORMAL:
        break;
    case HeapSlotState::DEAD:
        return HeapPageMarkDeadResult{
            .error = HeapPageMarkDeadError::ALREADY_DEAD,
        };
    case HeapSlotState::UNUSED:
    case HeapSlotState::REDIRECT_RESERVED:
        return HeapPageMarkDeadResult{
            .error = HeapPageMarkDeadError::INVALID_SLOT_STATE,
        };
    }

    updated_slot.state = HeapSlotState::DEAD;
    std::array<std::byte, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE> encoded_slot{};
    if (!EncodeHeapSlotEntry(encoded_slot, updated_slot)) {
        return HeapPageMarkDeadResult{
            .error = HeapPageMarkDeadError::PAGE_INVALID,
        };
    }

    std::ranges::copy(encoded_slot,
                      page_->Bytes().begin() + static_cast<std::ptrdiff_t>(slot_offset));
    return HeapPageMarkDeadResult{};
}

HeapPageCompactResult HeapPage::Compact() noexcept {
    const auto validation = Validate();
    if (!validation || !validation.heap_header.has_value()) {
        return HeapPageCompactResult{
            .error = HeapPageCompactError::PAGE_INVALID,
            .page_error = validation.error,
        };
    }

    const HeapPageHeader original_header = *validation.heap_header;
    std::array<HeapSlotEntry, MAX_HEAP_PAGE_SLOT_COUNT> planned_slots{};
    std::array<std::uint16_t, MAX_HEAP_PAGE_SLOT_COUNT> source_offsets{};
    std::array<std::uint16_t, MAX_HEAP_PAGE_SLOT_COUNT> source_lengths{};
    std::array<SlotId, MAX_HEAP_PAGE_SLOT_COUNT> live_order{};
    std::array<SlotId, MAX_HEAP_PAGE_SLOT_COUNT> range_order{};
    std::size_t live_count = 0;
    std::size_t range_count = 0;

    for (std::uint32_t index = 0; index < original_header.slot_count; ++index) {
        const auto slot_id = static_cast<SlotId>(index);
        const std::size_t slot_offset =
            HEAP_PAGE_SLOT_DIRECTORY_OFFSET + (index * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE);
        const auto decoded_slot = DecodeHeapSlotEntry(
            page_->Bytes().subspan(slot_offset, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE));
        if (!decoded_slot.entry.has_value()) {
            return HeapPageCompactResult{
                .error = HeapPageCompactError::PAGE_INVALID,
                .page_error = decoded_slot.error == HeapSlotEntryDecodeError::INVALID_SLOT_STATE
                                  ? HeapPageValidationError::INVALID_SLOT_STATE
                                  : HeapPageValidationError::SLOT_DIRECTORY_OUT_OF_BOUNDS,
                .slot_id = slot_id,
            };
        }

        const HeapSlotEntry slot = *decoded_slot.entry;
        planned_slots[index] = slot;
        source_offsets[index] = slot.tuple_offset;
        source_lengths[index] = slot.tuple_length;

        switch (slot.state) {
        case HeapSlotState::NORMAL:
            live_order[live_count++] = slot_id;
            break;
        case HeapSlotState::DEAD: {
            const std::size_t tuple_offset = slot.tuple_offset;
            const std::size_t tuple_length = slot.tuple_length;
            if (tuple_offset > PAGE_SIZE ||
                (tuple_length > 0 && (tuple_offset < original_header.upper ||
                                      tuple_length > PAGE_SIZE - tuple_offset))) {
                return HeapPageCompactResult{
                    .error = HeapPageCompactError::TUPLE_RANGE_OUT_OF_BOUNDS,
                    .slot_id = slot_id,
                };
            }
            planned_slots[index].tuple_offset = 0;
            planned_slots[index].tuple_length = 0;
            break;
        }
        case HeapSlotState::UNUSED:
        case HeapSlotState::REDIRECT_RESERVED:
            return HeapPageCompactResult{
                .error = HeapPageCompactError::UNSUPPORTED_SLOT_STATE,
                .slot_id = slot_id,
            };
        }

        if (source_lengths[index] > 0) {
            range_order[range_count++] = slot_id;
        }
    }

    auto ranges = std::span{range_order}.first(range_count);
    std::ranges::sort(ranges, [&](SlotId left, SlotId right) {
        if (source_offsets[left] != source_offsets[right]) {
            return source_offsets[left] < source_offsets[right];
        }
        return left < right;
    });
    for (std::size_t index = 1; index < ranges.size(); ++index) {
        const SlotId previous = ranges[index - 1];
        const SlotId current = ranges[index];
        const std::size_t previous_end =
            static_cast<std::size_t>(source_offsets[previous]) + source_lengths[previous];
        if (source_offsets[current] < previous_end) {
            return HeapPageCompactResult{
                .error = HeapPageCompactError::OVERLAPPING_TUPLE_RANGES,
                .slot_id = current,
                .other_slot_id = previous,
            };
        }
    }

    auto live_slots = std::span{live_order}.first(live_count);
    std::ranges::sort(live_slots, [&](SlotId left, SlotId right) {
        if (source_offsets[left] != source_offsets[right]) {
            return source_offsets[left] > source_offsets[right];
        }
        return left < right;
    });

    std::size_t new_upper = PAGE_SIZE;
    for (const SlotId slot_id : live_slots) {
        new_upper -= source_lengths[slot_id];
        if (new_upper < source_offsets[slot_id]) {
            return HeapPageCompactResult{
                .error = HeapPageCompactError::OVERLAPPING_TUPLE_RANGES,
                .slot_id = slot_id,
            };
        }
        planned_slots[slot_id].tuple_offset = static_cast<std::uint16_t>(new_upper);
    }
    if (new_upper < original_header.lower) {
        return HeapPageCompactResult{
            .error = HeapPageCompactError::PAGE_INVALID,
            .page_error = HeapPageValidationError::LOWER_AFTER_UPPER,
        };
    }

    HeapPageHeader updated_header = original_header;
    updated_header.upper = static_cast<std::uint16_t>(new_upper);
    std::array<std::array<std::byte, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE>, MAX_HEAP_PAGE_SLOT_COUNT>
        encoded_slots{};
    for (std::uint32_t index = 0; index < original_header.slot_count; ++index) {
        if (!EncodeHeapSlotEntry(encoded_slots[index], planned_slots[index])) {
            return HeapPageCompactResult{
                .error = HeapPageCompactError::PAGE_INVALID,
                .slot_id = static_cast<SlotId>(index),
            };
        }
    }
    std::array<std::byte, HEAP_PAGE_HEADER_ENCODED_SIZE> encoded_header{};
    if (!EncodeHeapPageHeader(encoded_header, updated_header)) {
        return HeapPageCompactResult{
            .error = HeapPageCompactError::PAGE_INVALID,
        };
    }

    auto page_bytes = page_->Bytes();
    for (const SlotId slot_id : live_slots) {
        const std::size_t tuple_length = source_lengths[slot_id];
        const std::size_t destination = planned_slots[slot_id].tuple_offset;
        const std::size_t source = source_offsets[slot_id];
        if (tuple_length > 0 && destination != source) {
            std::memmove(page_bytes.data() + destination, page_bytes.data() + source, tuple_length);
        }
    }
    for (std::uint32_t index = 0; index < original_header.slot_count; ++index) {
        const std::size_t slot_offset =
            HEAP_PAGE_SLOT_DIRECTORY_OFFSET + (index * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE);
        std::ranges::copy(encoded_slots[index],
                          page_bytes.begin() + static_cast<std::ptrdiff_t>(slot_offset));
    }
    std::ranges::copy(encoded_header,
                      page_bytes.begin() + static_cast<std::ptrdiff_t>(HEAP_PAGE_HEADER_OFFSET));

    return HeapPageCompactResult{};
}

std::optional<std::span<const std::byte>> HeapPage::TupleBytes(SlotId slot_id) const noexcept {
    const auto validation = Validate();
    if (!validation || !validation.heap_header.has_value() ||
        slot_id >= validation.heap_header->slot_count) {
        return std::nullopt;
    }

    const std::size_t slot_offset =
        HEAP_PAGE_SLOT_DIRECTORY_OFFSET +
        (static_cast<std::size_t>(slot_id) * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE);
    const Page& page = *page_;
    const auto decoded_slot =
        DecodeHeapSlotEntry(page.Bytes().subspan(slot_offset, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE));
    if (!decoded_slot.entry.has_value() || decoded_slot.entry->state != HeapSlotState::NORMAL) {
        return std::nullopt;
    }

    return page.Bytes().subspan(decoded_slot.entry->tuple_offset, decoded_slot.entry->tuple_length);
}

} // namespace dblusblus
