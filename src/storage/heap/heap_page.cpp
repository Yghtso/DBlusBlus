#include "storage/heap/heap_page.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <tuple>

namespace dblusblus {
namespace {

constexpr std::size_t MAX_HEAP_PAGE_SLOT_COUNT =
    (PAGE_SIZE - HEAP_PAGE_SLOT_DIRECTORY_OFFSET) / HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE;

[[nodiscard]] constexpr bool IsValidHeapPageNumber(PageNo page_no) noexcept {
    return page_no != 0 && page_no != INVALID_PAGE_NO;
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

// Canonical per-state slot fields and retained-range bounds (ARCHITECTURE.md §4.13.3).
[[nodiscard]] HeapPageValidationError ValidateSlotDescriptor(const HeapSlotEntry& slot,
                                                             std::uint16_t upper) noexcept {
    switch (slot.state) {
    case HeapSlotState::UNUSED:
        if (slot.tuple_offset != 0 || slot.tuple_length != 0) {
            return HeapPageValidationError::NONCANONICAL_UNUSED_SLOT;
        }
        return HeapPageValidationError::NONE;
    case HeapSlotState::REDIRECT_RESERVED:
        return HeapPageValidationError::UNSUPPORTED_RESERVED_STATE;
    case HeapSlotState::NORMAL:
    case HeapSlotState::DEAD:
        break;
    }

    if (slot.aux != 0) {
        return HeapPageValidationError::NONZERO_SLOT_AUX;
    }
    if (slot.state == HeapSlotState::DEAD) {
        const bool zero_offset = slot.tuple_offset == 0;
        const bool zero_length = slot.tuple_length == 0;
        if (zero_offset && zero_length) {
            return HeapPageValidationError::NONE; // Reclaimed DEAD form owns no bytes.
        }
        if (zero_offset != zero_length) {
            return HeapPageValidationError::NONCANONICAL_DEAD_SLOT;
        }
    } else if (slot.tuple_length == 0) {
        return HeapPageValidationError::EMPTY_NORMAL_TUPLE;
    }

    // A retained range lies wholly in [upper, PAGE_SIZE); since lower <= upper it cannot touch
    // the headers or the slot directory. Widened arithmetic cannot overflow.
    const std::size_t tuple_offset = slot.tuple_offset;
    const std::size_t tuple_end = tuple_offset + slot.tuple_length;
    if (tuple_offset < upper || tuple_end > PAGE_SIZE) {
        return HeapPageValidationError::RETAINED_TUPLE_OUT_OF_BOUNDS;
    }
    return HeapPageValidationError::NONE;
}

struct TupleCheck {
    HeapPageValidationError error{HeapPageValidationError::NONE};
    TupleCodecError tuple_error{TupleCodecError::NONE};
    TupleHeaderDecodeError tuple_header_error{TupleHeaderDecodeError::NONE};
};

// Schema-directed tuple validity plus the bounded tuple-header domain checks that need no
// status lookup or cross-page fetch. `rid` is where the tuple is (or will be) stored.
[[nodiscard]] TupleCheck ValidateTupleForSlot(std::span<const std::byte> tuple,
                                              const HeapTupleSchemaHistory& schema_history,
                                              Rid rid,
                                              std::uint16_t slot_count) noexcept {
    const auto decoded_header = DecodeTupleHeader(tuple);
    if (!decoded_header.header.has_value()) {
        return {
            .error = HeapPageValidationError::INVALID_TUPLE,
            .tuple_error = TupleCodecError::MALFORMED_TUPLE,
            .tuple_header_error = decoded_header.error,
        };
    }
    const TupleHeader& header = *decoded_header.header;

    const TuplePhysicalLayout* layout = schema_history.Resolve(header.schema_version);
    if (layout == nullptr) {
        return {.error = HeapPageValidationError::UNRESOLVED_SCHEMA_VERSION};
    }
    const auto tuple_validation = ValidateTuple(*layout, tuple);
    if (!tuple_validation) {
        return {
            .error = HeapPageValidationError::INVALID_TUPLE,
            .tuple_error = tuple_validation.error,
            .tuple_header_error = tuple_validation.header_error,
        };
    }

    // xmin is FROZEN or normal; xmax is INVALID or normal; no deleter means cmax == 0.
    const bool xmin_valid = header.xmin != INVALID_TXN_ID;
    const bool xmax_valid = header.xmax != FROZEN_TXN_ID;
    const bool cmax_valid = header.xmax != INVALID_TXN_ID || header.cmax == 0;
    if (!xmin_valid || !xmax_valid || !cmax_valid) {
        return {.error = HeapPageValidationError::INVALID_TUPLE_TRANSACTION_FIELDS};
    }

    // The tuple-header codec already enforces the both-or-neither sentinel pairing.
    if (header.prev_page_no != INVALID_PAGE_NO) {
        const bool ordinary_page = header.prev_page_no != 0;
        const bool same_page = header.prev_page_no == rid.page.page_no;
        const bool self_reference = same_page && header.prev_slot == rid.slot;
        const bool same_page_out_of_range = same_page && header.prev_slot >= slot_count;
        if (!ordinary_page || self_reference || same_page_out_of_range) {
            return {.error = HeapPageValidationError::INVALID_PREVIOUS_VERSION_POINTER};
        }
    }
    return {};
}

} // namespace

HeapTupleSchemaHistory::HeapTupleSchemaHistory(
    std::span<const TuplePhysicalLayout> layouts) noexcept
    : layouts_(layouts) {}

const TuplePhysicalLayout*
HeapTupleSchemaHistory::Resolve(SchemaVer schema_version) const noexcept {
    if (schema_version == 0) {
        return nullptr;
    }
    for (const TuplePhysicalLayout& layout : layouts_) {
        if (layout.SchemaVersion() == schema_version) {
            return &layout;
        }
    }
    return nullptr;
}

HeapPage::HeapPage(Page& page, HeapTupleSchemaHistory schema_history) noexcept
    : page_(&page), schema_history_(schema_history) {}

bool HeapPage::Initialize(Lsn page_lsn) noexcept {
    if (!IsValidHeapPageNumber(page_->Id().page_no)) {
        return false;
    }

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
    if (!IsValidHeapPageNumber(page_->Id().page_no)) {
        return ValidationFailure(HeapPageValidationError::INVALID_PAGE_NUMBER, common_header);
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

    // Pass 1: canonical slot descriptors and retained-range bounds.
    std::array<HeapSlotEntry, MAX_HEAP_PAGE_SLOT_COUNT> slots{};
    std::array<SlotId, MAX_HEAP_PAGE_SLOT_COUNT> retained_order{};
    std::size_t retained_count = 0;
    std::bitset<MAX_HEAP_PAGE_SLOT_COUNT> unused_slots;

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
        slots[index] = slot;
        const auto slot_error = ValidateSlotDescriptor(slot, heap_header->upper);
        if (slot_error != HeapPageValidationError::NONE) {
            return ValidationFailure(slot_error, common_header, heap_header, slot_id);
        }
        if (slot.state == HeapSlotState::UNUSED) {
            unused_slots.set(index);
        } else if (slot.tuple_length != 0) {
            retained_order[retained_count++] = slot_id;
        }
    }

    // Pass 2: no two retained NORMAL/DEAD ranges share a byte. Sorting by (start, end, SlotId)
    // makes the reported pair deterministic.
    auto retained = std::span{retained_order}.first(retained_count);
    std::ranges::sort(retained, [&](SlotId left, SlotId right) {
        const auto left_key = std::tuple{slots[left].tuple_offset, slots[left].tuple_length, left};
        const auto right_key =
            std::tuple{slots[right].tuple_offset, slots[right].tuple_length, right};
        return left_key < right_key;
    });
    for (std::size_t index = 1; index < retained.size(); ++index) {
        const SlotId previous = retained[index - 1];
        const SlotId current = retained[index];
        const std::size_t previous_end =
            static_cast<std::size_t>(slots[previous].tuple_offset) + slots[previous].tuple_length;
        if (slots[current].tuple_offset < previous_end) {
            auto failure = ValidationFailure(HeapPageValidationError::OVERLAPPING_TUPLE_RANGES,
                                             common_header,
                                             heap_header,
                                             current);
            failure.other_slot_id = previous;
            return failure;
        }
    }

    // Pass 3: every retained tuple, NORMAL or DEAD, is a complete valid tuple. SlotId order keeps
    // the first reported failure deterministic.
    for (std::uint32_t index = 0; index < heap_header->slot_count; ++index) {
        const HeapSlotEntry& slot = slots[index];
        if (slot.state == HeapSlotState::UNUSED || slot.tuple_length == 0) {
            continue;
        }
        const auto slot_id = static_cast<SlotId>(index);
        const auto tuple_check =
            ValidateTupleForSlot(page_->Bytes().subspan(slot.tuple_offset, slot.tuple_length),
                                 schema_history_,
                                 Rid{.page = page_->Id(), .slot = slot_id},
                                 heap_header->slot_count);
        if (tuple_check.error != HeapPageValidationError::NONE) {
            auto failure =
                ValidationFailure(tuple_check.error, common_header, heap_header, slot_id);
            failure.tuple_error = tuple_check.tuple_error;
            failure.tuple_header_error = tuple_check.tuple_header_error;
            return failure;
        }
    }

    // Pass 4: the UNUSED free-slot chain.
    for (std::uint32_t index = 0; index < heap_header->slot_count; ++index) {
        if (!unused_slots[index]) {
            continue;
        }
        const SlotId next_slot_id = slots[index].aux;
        if (next_slot_id != INVALID_SLOT_ID &&
            (next_slot_id >= heap_header->slot_count || !unused_slots[next_slot_id])) {
            return ValidationFailure(HeapPageValidationError::INVALID_FREE_SLOT_LINK,
                                     common_header,
                                     heap_header,
                                     static_cast<SlotId>(index));
        }
    }

    if (heap_header->free_slot_head != INVALID_SLOT_ID &&
        (heap_header->free_slot_head >= heap_header->slot_count ||
         !unused_slots[heap_header->free_slot_head])) {
        return ValidationFailure(HeapPageValidationError::INVALID_FREE_SLOT_HEAD,
                                 common_header,
                                 heap_header,
                                 heap_header->free_slot_head);
    }

    std::bitset<MAX_HEAP_PAGE_SLOT_COUNT> visited_free_slots;
    SlotId current_slot_id = heap_header->free_slot_head;
    while (current_slot_id != INVALID_SLOT_ID) {
        if (visited_free_slots[current_slot_id]) {
            return ValidationFailure(HeapPageValidationError::FREE_SLOT_CYCLE,
                                     common_header,
                                     heap_header,
                                     current_slot_id);
        }
        visited_free_slots.set(current_slot_id);
        current_slot_id = slots[current_slot_id].aux;
    }

    if (visited_free_slots != unused_slots) {
        for (std::uint32_t index = 0; index < heap_header->slot_count; ++index) {
            if (unused_slots[index] && !visited_free_slots[index]) {
                return ValidationFailure(HeapPageValidationError::FREE_SLOT_MEMBERSHIP_MISMATCH,
                                         common_header,
                                         heap_header,
                                         static_cast<SlotId>(index));
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

    // The after-image must pass the same owner validation future readers apply, so the new tuple
    // is validated as if already stored in its new slot.
    const SlotId new_slot_id = updated_header.slot_count;
    const auto tuple_check = ValidateTupleForSlot(tuple,
                                                  schema_history_,
                                                  Rid{.page = page_->Id(), .slot = new_slot_id},
                                                  static_cast<std::uint16_t>(new_slot_id + 1));
    if (tuple_check.error != HeapPageValidationError::NONE) {
        return HeapPageInsertResult{
            .rid = std::nullopt,
            .error = HeapPageInsertError::INVALID_TUPLE,
            .tuple_error = tuple_check.error,
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

    const SlotId slot_id = new_slot_id;
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
    std::memmove(page_bytes.data() + new_upper, tuple.data(), tuple.size());
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
    std::size_t live_count = 0;

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
        case HeapSlotState::DEAD:
            planned_slots[index].tuple_offset = 0;
            planned_slots[index].tuple_length = 0;
            break;
        case HeapSlotState::UNUSED:
        case HeapSlotState::REDIRECT_RESERVED:
            return HeapPageCompactResult{
                .error = HeapPageCompactError::UNSUPPORTED_SLOT_STATE,
                .slot_id = slot_id,
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
        // Validate() proved retained ranges disjoint and inside [upper, PAGE_SIZE), so packing
        // in descending source order only ever moves a payload toward the page end.
        assert(new_upper >= source_offsets[slot_id]);
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
