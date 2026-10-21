#include "storage/heap/heap_page.h"

#include "common/assert.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <tuple>
#include <utility>

namespace dblusblus::storage {
namespace {

constexpr std::size_t MAX_HEAP_PAGE_SLOT_COUNT =
    (PAGE_SIZE - HEAP_PAGE_SLOT_DIRECTORY_OFFSET) / HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE;

[[nodiscard]] constexpr bool IsValidHeapPageNumber(PageNo page_no) noexcept {
    return page_no != 0 && page_no != INVALID_PAGE_NO;
}

[[nodiscard]] HeapPageValidationResult
ValidationFailure(HeapPageValidationError error, SlotId slot_id = INVALID_SLOT_ID) noexcept {
    return Unexpected{HeapPageValidationFailure{.code = error, .slot_id = slot_id}};
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
    if (!decoded_header.Ok()) {
        return {
            .error = HeapPageValidationError::INVALID_TUPLE,
            .tuple_error = TupleCodecError::MALFORMED_TUPLE,
            .tuple_header_error = decoded_header.Error(),
        };
    }
    const TupleHeader& header = *decoded_header;

    const TuplePhysicalLayout* layout = schema_history.Resolve(header.schema_version);
    if (layout == nullptr) {
        return {.error = HeapPageValidationError::UNRESOLVED_SCHEMA_VERSION};
    }
    const auto tuple_validation = ValidateTuple(*layout, tuple);
    if (!tuple_validation) {
        return {
            .error = HeapPageValidationError::INVALID_TUPLE,
            .tuple_error = tuple_validation.Error().code,
            .tuple_header_error = tuple_validation.Error().header_error,
        };
    }

    // xmin is FROZEN or normal; xmax is INVALID or normal; no deleter means cmax == 0.
    const bool xmin_valid = header.xmin != INVALID_TXN_ID;
    const bool xmax_valid = header.xmax != FROZEN_TXN_ID;
    const bool cmax_valid = header.xmax != INVALID_TXN_ID || header.cmax == CommandId{0};
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

// Common-header fields a heap page must carry (ARCHITECTURE.md §4.13.3).
[[nodiscard]] HeapPageValidationError ValidateCommonHeader(const CommonPageHeader& header,
                                                           PageNo expected_page_no) noexcept {
    if (header.page_type != PageType::HEAP_DATA) {
        return HeapPageValidationError::WRONG_PAGE_TYPE;
    }
    if (header.page_no != expected_page_no) {
        return HeapPageValidationError::WRONG_PAGE_NUMBER;
    }
    if (!IsValidHeapPageNumber(expected_page_no)) {
        return HeapPageValidationError::INVALID_PAGE_NUMBER;
    }
    if (header.header_size != HEAP_PAGE_TOTAL_HEADER_SIZE) {
        return HeapPageValidationError::WRONG_HEADER_SIZE;
    }
    if (header.format_version != HEAP_PAGE_FORMAT_VERSION) {
        return HeapPageValidationError::UNSUPPORTED_FORMAT_VERSION;
    }
    if (header.flags != 0) {
        return HeapPageValidationError::NONZERO_COMMON_FLAGS;
    }
    if (header.reserved16 != 0) {
        return HeapPageValidationError::NONZERO_COMMON_RESERVED;
    }
    return HeapPageValidationError::NONE;
}

// lower/upper bounds and the slot count they imply.
[[nodiscard]] HeapPageValidationError
ValidateHeapHeaderGeometry(const HeapPageHeader& header) noexcept {
    if (header.reserved != 0) {
        return HeapPageValidationError::NONZERO_HEAP_RESERVED;
    }
    if (header.lower < HEAP_PAGE_TOTAL_HEADER_SIZE) {
        return HeapPageValidationError::LOWER_BEFORE_HEADER;
    }
    if (header.upper > PAGE_SIZE) {
        return HeapPageValidationError::UPPER_AFTER_PAGE;
    }
    if (header.lower > header.upper) {
        return HeapPageValidationError::LOWER_AFTER_UPPER;
    }
    const std::size_t slot_directory_size =
        static_cast<std::size_t>(header.slot_count) * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE;
    if (slot_directory_size > PAGE_SIZE - HEAP_PAGE_SLOT_DIRECTORY_OFFSET) {
        return HeapPageValidationError::SLOT_DIRECTORY_OUT_OF_BOUNDS;
    }
    if (header.lower != HEAP_PAGE_SLOT_DIRECTORY_OFFSET + slot_directory_size) {
        return HeapPageValidationError::SLOT_COUNT_LOWER_MISMATCH;
    }
    return HeapPageValidationError::NONE;
}

using HeapPageValidationStatus = Status<HeapPageValidationFailure>;

// Decoded slot directory shared by the validation passes.
struct SlotDirectory {
    std::uint16_t slot_count{0};
    std::array<HeapSlotEntry, MAX_HEAP_PAGE_SLOT_COUNT> slots{};
    // Slots that own tuple bytes (NORMAL, or DEAD not yet reclaimed).
    std::array<SlotId, MAX_HEAP_PAGE_SLOT_COUNT> retained_order{};
    std::size_t retained_count{0};
    std::bitset<MAX_HEAP_PAGE_SLOT_COUNT> unused_slots;
};

[[nodiscard]] HeapPageValidationStatus SlotFailure(HeapPageValidationError error,
                                                   SlotId slot_id) noexcept {
    return Unexpected{HeapPageValidationFailure{.code = error, .slot_id = slot_id}};
}

// Pass 1: canonical slot descriptors and retained-range bounds.
[[nodiscard]] HeapPageValidationStatus DecodeSlotDirectory(std::span<const std::byte> page_bytes,
                                                           std::uint16_t upper,
                                                           SlotDirectory& directory) noexcept {
    for (std::uint32_t index = 0; index < directory.slot_count; ++index) {
        const std::size_t slot_offset =
            HEAP_PAGE_SLOT_DIRECTORY_OFFSET + (index * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE);
        const auto decoded_slot =
            DecodeHeapSlotEntry(page_bytes.subspan(slot_offset, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE));
        const auto slot_id = static_cast<SlotId>(index);
        if (!decoded_slot.Ok()) {
            return SlotFailure(decoded_slot.Error() == HeapSlotEntryDecodeError::INVALID_SLOT_STATE
                                   ? HeapPageValidationError::INVALID_SLOT_STATE
                                   : HeapPageValidationError::SLOT_DIRECTORY_OUT_OF_BOUNDS,
                               slot_id);
        }

        const HeapSlotEntry& slot = *decoded_slot;
        directory.slots[index] = slot;
        if (const auto error = ValidateSlotDescriptor(slot, upper);
            error != HeapPageValidationError::NONE) {
            return SlotFailure(error, slot_id);
        }
        if (slot.state == HeapSlotState::UNUSED) {
            directory.unused_slots.set(index);
        } else if (slot.tuple_length != 0) {
            directory.retained_order[directory.retained_count++] = slot_id;
        }
    }
    return {};
}

// Pass 2: no two retained NORMAL/DEAD ranges share a byte. Sorting by (start, end, SlotId) makes
// the reported pair deterministic.
[[nodiscard]] HeapPageValidationStatus
CheckRetainedRangesDisjoint(SlotDirectory& directory) noexcept {
    const auto& slots = directory.slots;
    auto retained = std::span{directory.retained_order}.first(directory.retained_count);
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
            return Unexpected{HeapPageValidationFailure{
                .code = HeapPageValidationError::OVERLAPPING_TUPLE_RANGES,
                .slot_id = current,
                .other_slot_id = previous,
            }};
        }
    }
    return {};
}

// Pass 3: every retained tuple, NORMAL or DEAD, is a complete valid tuple. SlotId order keeps the
// first reported failure deterministic.
[[nodiscard]] HeapPageValidationStatus
ValidateRetainedTuples(std::span<const std::byte> page_bytes,
                       PageId page_id,
                       const HeapTupleSchemaHistory& schema_history,
                       const SlotDirectory& directory) noexcept {
    for (std::uint32_t index = 0; index < directory.slot_count; ++index) {
        const HeapSlotEntry& slot = directory.slots[index];
        if (slot.state == HeapSlotState::UNUSED || slot.tuple_length == 0) {
            continue;
        }
        const auto slot_id = static_cast<SlotId>(index);
        const auto tuple_check =
            ValidateTupleForSlot(page_bytes.subspan(slot.tuple_offset, slot.tuple_length),
                                 schema_history,
                                 Rid{.page = page_id, .slot = slot_id},
                                 directory.slot_count);
        if (tuple_check.error != HeapPageValidationError::NONE) {
            return Unexpected{HeapPageValidationFailure{
                .code = tuple_check.error,
                .slot_id = slot_id,
                .tuple_error = tuple_check.tuple_error,
                .tuple_header_error = tuple_check.tuple_header_error,
            }};
        }
    }
    return {};
}

// Pass 4: the UNUSED free-slot chain links only UNUSED slots, has no cycle, and reaches every one.
[[nodiscard]] HeapPageValidationStatus ValidateFreeSlotChain(const SlotDirectory& directory,
                                                             SlotId free_slot_head) noexcept {
    const auto& slots = directory.slots;
    const auto& unused_slots = directory.unused_slots;
    const auto is_unused = [&](SlotId slot_id) {
        return slot_id < directory.slot_count && unused_slots[slot_id];
    };

    for (std::uint32_t index = 0; index < directory.slot_count; ++index) {
        if (!unused_slots[index]) {
            continue;
        }
        const SlotId next_slot_id = slots[index].aux;
        if (next_slot_id != INVALID_SLOT_ID && !is_unused(next_slot_id)) {
            return SlotFailure(HeapPageValidationError::INVALID_FREE_SLOT_LINK,
                               static_cast<SlotId>(index));
        }
    }
    if (free_slot_head != INVALID_SLOT_ID && !is_unused(free_slot_head)) {
        return SlotFailure(HeapPageValidationError::INVALID_FREE_SLOT_HEAD, free_slot_head);
    }

    std::bitset<MAX_HEAP_PAGE_SLOT_COUNT> visited_free_slots;
    for (SlotId current = free_slot_head; current != INVALID_SLOT_ID;
         current = slots[current].aux) {
        if (visited_free_slots[current]) {
            return SlotFailure(HeapPageValidationError::FREE_SLOT_CYCLE, current);
        }
        visited_free_slots.set(current);
    }

    if (visited_free_slots != unused_slots) {
        for (std::uint32_t index = 0; index < directory.slot_count; ++index) {
            if (unused_slots[index] && !visited_free_slots[index]) {
                return SlotFailure(HeapPageValidationError::FREE_SLOT_MEMBERSHIP_MISMATCH,
                                   static_cast<SlotId>(index));
            }
        }
    }
    return {};
}

// Where every slot's tuple goes after compaction. Built without touching the page.
struct CompactionPlan {
    std::uint16_t slot_count{0};
    std::array<HeapSlotEntry, MAX_HEAP_PAGE_SLOT_COUNT> planned_slots{};
    std::array<std::uint16_t, MAX_HEAP_PAGE_SLOT_COUNT> source_offsets{};
    std::array<std::uint16_t, MAX_HEAP_PAGE_SLOT_COUNT> source_lengths{};
    // NORMAL slots, in descending source-offset order once planned.
    std::array<SlotId, MAX_HEAP_PAGE_SLOT_COUNT> live_order{};
    std::size_t live_count{0};
    std::size_t new_upper{PAGE_SIZE};

    [[nodiscard]] std::span<const SlotId> LiveSlots() const noexcept {
        return std::span{live_order}.first(live_count);
    }
};

using EncodedSlotDirectory =
    std::array<std::array<std::byte, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE>, MAX_HEAP_PAGE_SLOT_COUNT>;

// Reads the slot directory of an already validated page and packs NORMAL tuples against the
// page end, keeping their relative order. DEAD slots give up their bytes.
[[nodiscard]] HeapPageCompactResult PlanCompaction(std::span<const std::byte> page_bytes,
                                                   std::uint16_t lower,
                                                   CompactionPlan& plan) noexcept {
    for (std::uint32_t index = 0; index < plan.slot_count; ++index) {
        const auto slot_id = static_cast<SlotId>(index);
        const std::size_t slot_offset =
            HEAP_PAGE_SLOT_DIRECTORY_OFFSET + (index * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE);
        const auto decoded_slot =
            DecodeHeapSlotEntry(page_bytes.subspan(slot_offset, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE));
        if (!decoded_slot.Ok()) {
            return Unexpected{HeapPageCompactFailure{
                .code = HeapPageCompactError::PAGE_INVALID,
                .page_error = decoded_slot.Error() == HeapSlotEntryDecodeError::INVALID_SLOT_STATE
                                  ? HeapPageValidationError::INVALID_SLOT_STATE
                                  : HeapPageValidationError::SLOT_DIRECTORY_OUT_OF_BOUNDS,
                .slot_id = slot_id,
            }};
        }

        const HeapSlotEntry slot = *decoded_slot;
        plan.planned_slots[index] = slot;
        plan.source_offsets[index] = slot.tuple_offset;
        plan.source_lengths[index] = slot.tuple_length;

        switch (slot.state) {
        case HeapSlotState::NORMAL:
            plan.live_order[plan.live_count++] = slot_id;
            break;
        case HeapSlotState::DEAD:
            plan.planned_slots[index].tuple_offset = 0;
            plan.planned_slots[index].tuple_length = 0;
            break;
        case HeapSlotState::UNUSED:
        case HeapSlotState::REDIRECT_RESERVED:
            return Unexpected{HeapPageCompactFailure{
                .code = HeapPageCompactError::UNSUPPORTED_SLOT_STATE,
                .slot_id = slot_id,
            }};
        }
    }

    auto live_slots = std::span{plan.live_order}.first(plan.live_count);
    std::ranges::sort(live_slots, [&](SlotId left, SlotId right) {
        if (plan.source_offsets[left] != plan.source_offsets[right]) {
            return plan.source_offsets[left] > plan.source_offsets[right];
        }
        return left < right;
    });

    plan.new_upper = PAGE_SIZE;
    for (const SlotId slot_id : live_slots) {
        plan.new_upper -= plan.source_lengths[slot_id];
        // Validate() proved retained ranges disjoint and inside [upper, PAGE_SIZE), so packing
        // in descending source order only ever moves a payload toward the page end.
        DBLUS_ASSERT(plan.new_upper >= plan.source_offsets[slot_id],
                     "compaction must move tuples toward the page end");
        plan.planned_slots[slot_id].tuple_offset = static_cast<std::uint16_t>(plan.new_upper);
    }
    if (plan.new_upper < lower) {
        return Unexpected{HeapPageCompactFailure{
            .code = HeapPageCompactError::PAGE_INVALID,
            .page_error = HeapPageValidationError::LOWER_AFTER_UPPER,
        }};
    }
    return {};
}

[[nodiscard]] HeapPageCompactResult EncodeCompactedMetadata(
    const CompactionPlan& plan,
    const HeapPageHeader& updated_header,
    EncodedSlotDirectory& encoded_slots,
    std::array<std::byte, HEAP_PAGE_HEADER_ENCODED_SIZE>& encoded_header) noexcept {
    for (std::uint32_t index = 0; index < plan.slot_count; ++index) {
        if (!EncodeHeapSlotEntry(encoded_slots[index], plan.planned_slots[index])) {
            return Unexpected{HeapPageCompactFailure{
                .code = HeapPageCompactError::PAGE_INVALID,
                .slot_id = static_cast<SlotId>(index),
            }};
        }
    }
    if (!EncodeHeapPageHeader(encoded_header, updated_header)) {
        return Unexpected{HeapPageCompactFailure{.code = HeapPageCompactError::PAGE_INVALID}};
    }
    return {};
}

// Moves the tuples and writes the new slot directory and header. Cannot fail.
void ApplyCompaction(
    std::span<std::byte, PAGE_SIZE> page_bytes,
    const CompactionPlan& plan,
    const EncodedSlotDirectory& encoded_slots,
    const std::array<std::byte, HEAP_PAGE_HEADER_ENCODED_SIZE>& encoded_header) noexcept {
    for (const SlotId slot_id : plan.LiveSlots()) {
        const std::size_t tuple_length = plan.source_lengths[slot_id];
        const std::size_t destination = plan.planned_slots[slot_id].tuple_offset;
        const std::size_t source = plan.source_offsets[slot_id];
        if (tuple_length > 0 && destination != source) {
            std::memmove(page_bytes.data() + destination, page_bytes.data() + source, tuple_length);
        }
    }
    for (std::uint32_t index = 0; index < plan.slot_count; ++index) {
        const std::size_t slot_offset =
            HEAP_PAGE_SLOT_DIRECTORY_OFFSET + (index * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE);
        std::ranges::copy(encoded_slots[index],
                          page_bytes.begin() + static_cast<std::ptrdiff_t>(slot_offset));
    }
    std::ranges::copy(encoded_header,
                      page_bytes.begin() + static_cast<std::ptrdiff_t>(HEAP_PAGE_HEADER_OFFSET));
}

} // namespace

HeapTupleSchemaHistory::HeapTupleSchemaHistory(
    std::span<const TuplePhysicalLayout> layouts) noexcept
    : layouts_(layouts) {}

const TuplePhysicalLayout*
HeapTupleSchemaHistory::Resolve(SchemaVer schema_version) const noexcept {
    if (schema_version == SchemaVer{0}) {
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

Status<HeapPageInitializeError> HeapPage::Initialize(Lsn page_lsn) noexcept {
    if (!IsValidHeapPageNumber(page_->Id().page_no)) {
        return Unexpected{HeapPageInitializeError::INVALID_PAGE_NUMBER};
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
    page_->Initialize(common_header);

    const bool encoded = EncodeHeapPageHeader(
        page_->Bytes().subspan(HEAP_PAGE_HEADER_OFFSET, HEAP_PAGE_HEADER_ENCODED_SIZE),
        HeapPageHeader{});
    DBLUS_CHECK(encoded, "heap page header must fit in a page");
    return {};
}

std::optional<HeapPageHeader> HeapPage::Header() const noexcept {
    return DecodeHeapPageHeader(
        page_->Bytes().subspan(HEAP_PAGE_HEADER_OFFSET, HEAP_PAGE_HEADER_ENCODED_SIZE));
}

HeapPageValidationResult HeapPage::Validate() const noexcept {
    const auto common_header = page_->DecodeHeader();
    if (!common_header.has_value()) {
        return ValidationFailure(HeapPageValidationError::COMMON_HEADER_DECODE_FAILED);
    }
    if (const auto error = ValidateCommonHeader(*common_header, page_->Id().page_no);
        error != HeapPageValidationError::NONE) {
        return ValidationFailure(error);
    }

    const auto heap_header = Header();
    if (!heap_header.has_value()) {
        return ValidationFailure(HeapPageValidationError::HEAP_HEADER_DECODE_FAILED);
    }
    if (const auto error = ValidateHeapHeaderGeometry(*heap_header);
        error != HeapPageValidationError::NONE) {
        return ValidationFailure(error);
    }

    // The passes run in this order so the first reported failure is deterministic.
    const std::span<const std::byte, PAGE_SIZE> bytes = page_->Bytes();
    SlotDirectory directory{};
    directory.slot_count = heap_header->slot_count;
    if (auto status = DecodeSlotDirectory(bytes, heap_header->upper, directory); !status) {
        return Unexpected{status.Error()};
    }
    if (auto status = CheckRetainedRangesDisjoint(directory); !status) {
        return Unexpected{status.Error()};
    }
    if (auto status = ValidateRetainedTuples(bytes, page_->Id(), schema_history_, directory);
        !status) {
        return Unexpected{status.Error()};
    }
    if (auto status = ValidateFreeSlotChain(directory, heap_header->free_slot_head); !status) {
        return Unexpected{status.Error()};
    }
    return HeapPageHeaders{.common = *common_header, .heap = *heap_header};
}

HeapPageInsertResult HeapPage::Insert(std::span<const std::byte> tuple) noexcept {
    const auto validation = Validate();
    if (!validation) {
        return Unexpected{HeapPageInsertFailure{.code = HeapPageInsertError::PAGE_INVALID,
                                                .page_error = validation.Error().code}};
    }
    if (tuple.size() > std::numeric_limits<std::uint16_t>::max() ||
        tuple.size() > HEAP_PAGE_MAX_RAW_TUPLE_SIZE) {
        return Unexpected{HeapPageInsertError::TUPLE_TOO_LARGE};
    }

    HeapPageHeader updated_header = (*validation).heap;
    if (updated_header.slot_count == std::numeric_limits<std::uint16_t>::max()) {
        return Unexpected{HeapPageInsertError::SLOT_ID_EXHAUSTED};
    }

    // The after-image must pass the same owner validation future readers apply, so the new tuple
    // is validated as if already stored in its new slot.
    const SlotId new_slot_id = updated_header.slot_count;
    const auto tuple_check = ValidateTupleForSlot(tuple,
                                                  schema_history_,
                                                  Rid{.page = page_->Id(), .slot = new_slot_id},
                                                  static_cast<std::uint16_t>(new_slot_id + 1));
    if (tuple_check.error != HeapPageValidationError::NONE) {
        return Unexpected{HeapPageInsertFailure{.code = HeapPageInsertError::INVALID_TUPLE,
                                                .tuple_error = tuple_check.error}};
    }

    const std::size_t available_space =
        static_cast<std::size_t>(updated_header.upper) - updated_header.lower;
    const std::size_t required_space = tuple.size() + HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE;
    if (required_space > available_space) {
        return Unexpected{HeapPageInsertError::INSUFFICIENT_SPACE};
    }

    const SlotId slot_id = new_slot_id;
    const std::size_t slot_offset = updated_header.lower;
    const std::size_t new_lower = slot_offset + HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE;
    const std::size_t new_upper = static_cast<std::size_t>(updated_header.upper) - tuple.size();
    if (new_lower > std::numeric_limits<std::uint16_t>::max() ||
        new_upper > std::numeric_limits<std::uint16_t>::max()) {
        return Unexpected{HeapPageInsertFailure{
            .code = HeapPageInsertError::PAGE_INVALID,
            .page_error = HeapPageValidationError::SLOT_DIRECTORY_OUT_OF_BOUNDS,
        }};
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
        return Unexpected{HeapPageInsertError::PAGE_INVALID};
    }

    auto page_bytes = page_->Bytes();
    std::memmove(page_bytes.data() + new_upper, tuple.data(), tuple.size());
    std::ranges::copy(encoded_slot, page_bytes.begin() + static_cast<std::ptrdiff_t>(slot_offset));
    std::ranges::copy(encoded_header,
                      page_bytes.begin() + static_cast<std::ptrdiff_t>(HEAP_PAGE_HEADER_OFFSET));

    return Rid{.page = page_->Id(), .slot = slot_id};
}

HeapPageMarkDeadResult HeapPage::MarkDead(SlotId slot_id) noexcept {
    const auto validation = Validate();
    if (!validation) {
        return Unexpected{HeapPageMarkDeadFailure{
            .code = HeapPageMarkDeadError::PAGE_INVALID,
            .page_error = validation.Error().code,
        }};
    }
    if (slot_id >= validation->heap.slot_count) {
        return Unexpected{HeapPageMarkDeadFailure{
            .code = HeapPageMarkDeadError::SLOT_OUT_OF_RANGE,
        }};
    }

    const std::size_t slot_offset =
        HEAP_PAGE_SLOT_DIRECTORY_OFFSET +
        (static_cast<std::size_t>(slot_id) * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE);
    const auto decoded_slot =
        DecodeHeapSlotEntry(page_->Bytes().subspan(slot_offset, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE));
    if (!decoded_slot.Ok()) {
        return Unexpected{HeapPageMarkDeadFailure{
            .code = HeapPageMarkDeadError::PAGE_INVALID,
            .page_error = decoded_slot.Error() == HeapSlotEntryDecodeError::INVALID_SLOT_STATE
                              ? HeapPageValidationError::INVALID_SLOT_STATE
                              : HeapPageValidationError::SLOT_DIRECTORY_OUT_OF_BOUNDS,
        }};
    }

    HeapSlotEntry updated_slot = *decoded_slot;
    switch (updated_slot.state) {
    case HeapSlotState::NORMAL:
        break;
    case HeapSlotState::DEAD:
        return Unexpected{HeapPageMarkDeadFailure{
            .code = HeapPageMarkDeadError::ALREADY_DEAD,
        }};
    case HeapSlotState::UNUSED:
    case HeapSlotState::REDIRECT_RESERVED:
        return Unexpected{HeapPageMarkDeadFailure{
            .code = HeapPageMarkDeadError::INVALID_SLOT_STATE,
        }};
    }

    updated_slot.state = HeapSlotState::DEAD;
    std::array<std::byte, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE> encoded_slot{};
    if (!EncodeHeapSlotEntry(encoded_slot, updated_slot)) {
        return Unexpected{HeapPageMarkDeadFailure{
            .code = HeapPageMarkDeadError::PAGE_INVALID,
        }};
    }

    std::ranges::copy(encoded_slot,
                      page_->Bytes().begin() + static_cast<std::ptrdiff_t>(slot_offset));
    return {};
}

HeapPageCompactResult HeapPage::Compact() noexcept {
    const auto validation = Validate();
    if (!validation) {
        return Unexpected{HeapPageCompactFailure{
            .code = HeapPageCompactError::PAGE_INVALID,
            .page_error = validation.Error().code,
        }};
    }

    // Plan and encode everything first, so a failure leaves the page unchanged.
    const HeapPageHeader& original_header = (*validation).heap;
    CompactionPlan plan{.slot_count = original_header.slot_count};
    if (auto status = PlanCompaction(page_->Bytes(), original_header.lower, plan); !status) {
        return status;
    }
    HeapPageHeader updated_header = original_header;
    updated_header.upper = static_cast<std::uint16_t>(plan.new_upper);
    EncodedSlotDirectory encoded_slots{};
    std::array<std::byte, HEAP_PAGE_HEADER_ENCODED_SIZE> encoded_header{};
    if (auto status = EncodeCompactedMetadata(plan, updated_header, encoded_slots, encoded_header);
        !status) {
        return status;
    }

    ApplyCompaction(page_->Bytes(), plan, encoded_slots, encoded_header);
    return HeapPageCompactResult{};
}

std::optional<std::span<const std::byte>> HeapPage::TupleBytes(SlotId slot_id) const noexcept {
    const auto validation = Validate();
    if (!validation || !validation.Ok() || slot_id >= validation->heap.slot_count) {
        return std::nullopt;
    }

    const std::size_t slot_offset =
        HEAP_PAGE_SLOT_DIRECTORY_OFFSET +
        (static_cast<std::size_t>(slot_id) * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE);
    const Page& page = *page_;
    const auto decoded_slot =
        DecodeHeapSlotEntry(page.Bytes().subspan(slot_offset, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE));
    if (!decoded_slot.Ok() || decoded_slot->state != HeapSlotState::NORMAL) {
        return std::nullopt;
    }

    return page.Bytes().subspan(decoded_slot->tuple_offset, decoded_slot->tuple_length);
}

} // namespace dblusblus::storage
