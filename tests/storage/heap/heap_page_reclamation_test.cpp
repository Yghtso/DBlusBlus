#include "common/encoding.h"
#include "storage/heap/heap_page.h"
#include "storage/page/page.h"
#include "storage/page/page_header.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <span>

namespace dblusblus {
namespace {

[[nodiscard]] std::span<std::byte> HeapHeaderBytes(Page& page) {
    return page.Bytes().subspan(HEAP_PAGE_HEADER_OFFSET, HEAP_PAGE_HEADER_ENCODED_SIZE);
}

[[nodiscard]] std::span<std::byte> SlotBytes(Page& page, SlotId slot_id) {
    return page.Bytes().subspan(
        HEAP_PAGE_SLOT_DIRECTORY_OFFSET +
            (static_cast<std::size_t>(slot_id) * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE),
        HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE);
}

[[nodiscard]] Page::ByteStorage CopyPageBytes(const Page& page) {
    Page::ByteStorage copy{};
    std::ranges::copy(page.Bytes(), copy.begin());
    return copy;
}

void WriteHeapHeader(Page& page, const HeapPageHeader& header) {
    ASSERT_TRUE(EncodeHeapPageHeader(HeapHeaderBytes(page), header));
}

void WriteCommonHeader(Page& page, const CommonPageHeader& header) {
    ASSERT_TRUE(page.WriteHeader(header));
}

[[nodiscard]] Page InitializedHeapPage(PageId page_id = {.file_id = 7, .page_no = 3}) {
    Page page{page_id};
    HeapPage heap_page{page};
    if (!heap_page.Initialize()) {
        ADD_FAILURE() << "heap page initialization unexpectedly failed";
    }
    return page;
}

void ConfigureOneNormalSlot(Page& page,
                            std::uint16_t tuple_offset = 8000,
                            std::uint16_t tuple_length = 192) {
    WriteHeapHeader(page,
                    HeapPageHeader{
                        .slot_count = 1,
                        .free_slot_head = INVALID_SLOT_ID,
                        .lower = HEAP_PAGE_TOTAL_HEADER_SIZE + HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE,
                        .upper = 8000,
                        .prune_hint = 0,
                        .reserved = 0,
                    });
    ASSERT_TRUE(EncodeHeapSlotEntry(SlotBytes(page, 0),
                                    HeapSlotEntry{
                                        .tuple_offset = tuple_offset,
                                        .tuple_length = tuple_length,
                                        .state = HeapSlotState::NORMAL,
                                        .aux = 0,
                                    }));
}

[[nodiscard]] constexpr HeapSlotEntry
CanonicalUnusedSlot(SlotId next_slot_id = INVALID_SLOT_ID) noexcept {
    return HeapSlotEntry{
        .tuple_offset = 0,
        .tuple_length = 0,
        .state = HeapSlotState::UNUSED,
        .aux = next_slot_id,
    };
}

void ConfigureSlotsForValidation(Page& page,
                                 std::span<const HeapSlotEntry> slots,
                                 SlotId free_slot_head = INVALID_SLOT_ID) {
    constexpr std::size_t maximum_slot_count =
        (PAGE_SIZE - HEAP_PAGE_SLOT_DIRECTORY_OFFSET) / HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE;
    ASSERT_LE(slots.size(), maximum_slot_count);

    const auto slot_count = static_cast<std::uint16_t>(slots.size());
    const auto lower = static_cast<std::uint16_t>(
        HEAP_PAGE_SLOT_DIRECTORY_OFFSET + (slots.size() * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE));
    WriteHeapHeader(page,
                    HeapPageHeader{
                        .slot_count = slot_count,
                        .free_slot_head = free_slot_head,
                        .lower = lower,
                        .upper = PAGE_SIZE,
                    });
    for (std::size_t index = 0; index < slots.size(); ++index) {
        ASSERT_TRUE(EncodeHeapSlotEntry(SlotBytes(page, static_cast<SlotId>(index)), slots[index]));
    }
}

TEST(HeapPageDeadTransitionTest, MarksNormalSlotDeadWithoutReclaimingBytesOrIdentity) {
    Page page = InitializedHeapPage(PageId{.file_id = 31, .page_no = 37});
    HeapPage heap_page{page};
    constexpr std::array tuple{std::byte{0x00}, std::byte{0x41}, std::byte{0x80}, std::byte{0xFF}};
    const auto insertion = heap_page.Insert(tuple);
    if (!insertion.rid.has_value()) {
        ADD_FAILURE() << "tuple insertion did not return a RID";
        return;
    }
    const Rid original_rid = *insertion.rid;

    auto original_slot = DecodeHeapSlotEntry(SlotBytes(page, original_rid.slot));
    if (!original_slot.entry.has_value()) {
        ADD_FAILURE() << "inserted slot did not decode";
        return;
    }
    original_slot.entry->aux = 0xA55A;
    ASSERT_TRUE(EncodeHeapSlotEntry(SlotBytes(page, original_rid.slot), *original_slot.entry));
    const HeapSlotEntry expected_preserved_slot = *original_slot.entry;

    const auto original_header = heap_page.Header();
    if (!original_header.has_value()) {
        ADD_FAILURE() << "heap header did not decode before transition";
        return;
    }
    const auto before = CopyPageBytes(page);
    const auto before_tuple = heap_page.TupleBytes(original_rid.slot);
    if (!before_tuple.has_value()) {
        ADD_FAILURE() << "NORMAL tuple was not retrievable before transition";
        return;
    }
    EXPECT_TRUE(std::ranges::equal(*before_tuple, tuple));

    const auto result = heap_page.MarkDead(original_rid.slot);
    EXPECT_TRUE(result);
    EXPECT_EQ(result.error, HeapPageMarkDeadError::NONE);

    const auto dead_slot = DecodeHeapSlotEntry(SlotBytes(page, original_rid.slot));
    if (!dead_slot.entry.has_value()) {
        ADD_FAILURE() << "DEAD slot did not decode";
        return;
    }
    EXPECT_EQ(dead_slot.entry->state, HeapSlotState::DEAD);
    EXPECT_EQ(dead_slot.entry->tuple_offset, expected_preserved_slot.tuple_offset);
    EXPECT_EQ(dead_slot.entry->tuple_length, expected_preserved_slot.tuple_length);
    EXPECT_EQ(dead_slot.entry->aux, expected_preserved_slot.aux);

    const auto current_header = heap_page.Header();
    if (!current_header.has_value()) {
        ADD_FAILURE() << "heap header did not decode after transition";
        return;
    }
    EXPECT_EQ(*current_header, *original_header);
    EXPECT_EQ(current_header->free_slot_head, INVALID_SLOT_ID);
    EXPECT_EQ(original_rid, (Rid{.page = page.Id(), .slot = 0}));
    EXPECT_FALSE(heap_page.TupleBytes(original_rid.slot).has_value());
    EXPECT_TRUE(std::ranges::equal(
        page.Bytes().subspan(dead_slot.entry->tuple_offset, dead_slot.entry->tuple_length), tuple));

    const auto after = CopyPageBytes(page);
    const std::size_t state_byte_offset = HEAP_PAGE_SLOT_DIRECTORY_OFFSET + HEAP_SLOT_FLAGS_OFFSET;
    for (std::size_t index = 0; index < PAGE_SIZE; ++index) {
        if (index == state_byte_offset) {
            EXPECT_EQ(after[index], std::byte{0x02});
        } else {
            EXPECT_EQ(after[index], before[index]);
        }
    }
}

TEST(HeapPageDeadTransitionTest, LeavesNeighboringNormalSlotsAndRidsUnchanged) {
    Page page = InitializedHeapPage();
    HeapPage heap_page{page};
    constexpr std::array first{std::byte{0x10}, std::byte{0x11}};
    constexpr std::array second{std::byte{0x20}, std::byte{0x21}, std::byte{0x22}};
    constexpr std::array third{std::byte{0x30}, std::byte{0x31}};
    const auto first_insert = heap_page.Insert(first);
    const auto second_insert = heap_page.Insert(second);
    const auto third_insert = heap_page.Insert(third);
    if (!first_insert.rid.has_value() || !second_insert.rid.has_value() ||
        !third_insert.rid.has_value()) {
        ADD_FAILURE() << "sequential insertion failed";
        return;
    }
    const Rid first_rid = *first_insert.rid;
    const Rid second_rid = *second_insert.rid;
    const Rid third_rid = *third_insert.rid;
    const auto first_slot_before = DecodeHeapSlotEntry(SlotBytes(page, first_rid.slot));
    const auto second_slot_before = DecodeHeapSlotEntry(SlotBytes(page, second_rid.slot));
    const auto third_slot_before = DecodeHeapSlotEntry(SlotBytes(page, third_rid.slot));
    if (!first_slot_before.entry.has_value() || !second_slot_before.entry.has_value() ||
        !third_slot_before.entry.has_value()) {
        ADD_FAILURE() << "one of the inserted slots did not decode";
        return;
    }

    ASSERT_TRUE(heap_page.MarkDead(second_rid.slot));

    const auto first_slot_after = DecodeHeapSlotEntry(SlotBytes(page, first_rid.slot));
    const auto second_slot_after = DecodeHeapSlotEntry(SlotBytes(page, second_rid.slot));
    const auto third_slot_after = DecodeHeapSlotEntry(SlotBytes(page, third_rid.slot));
    if (!first_slot_after.entry.has_value() || !second_slot_after.entry.has_value() ||
        !third_slot_after.entry.has_value()) {
        ADD_FAILURE() << "one of the transitioned slots did not decode";
        return;
    }
    EXPECT_EQ(*first_slot_after.entry, *first_slot_before.entry);
    EXPECT_EQ(*third_slot_after.entry, *third_slot_before.entry);
    EXPECT_EQ(second_slot_after.entry->tuple_offset, second_slot_before.entry->tuple_offset);
    EXPECT_EQ(second_slot_after.entry->tuple_length, second_slot_before.entry->tuple_length);
    EXPECT_EQ(second_slot_after.entry->aux, second_slot_before.entry->aux);
    EXPECT_EQ(second_slot_after.entry->state, HeapSlotState::DEAD);

    const auto stored_first = heap_page.TupleBytes(first_rid.slot);
    const auto stored_third = heap_page.TupleBytes(third_rid.slot);
    if (!stored_first.has_value() || !stored_third.has_value()) {
        ADD_FAILURE() << "neighboring NORMAL tuple was not retrievable";
        return;
    }
    EXPECT_TRUE(std::ranges::equal(*stored_first, first));
    EXPECT_FALSE(heap_page.TupleBytes(second_rid.slot).has_value());
    EXPECT_TRUE(std::ranges::equal(*stored_third, third));
    EXPECT_EQ(first_rid.slot, SlotId{0});
    EXPECT_EQ(second_rid.slot, SlotId{1});
    EXPECT_EQ(third_rid.slot, SlotId{2});
}

TEST(HeapPageDeadTransitionTest, RejectsDisallowedTransitionsWithoutMutation) {
    const auto expect_unchanged = [](Page& page,
                                     SlotId slot_id,
                                     HeapPageMarkDeadError expected_error,
                                     HeapPageValidationError expected_page_error =
                                         HeapPageValidationError::NONE) {
        const auto before = CopyPageBytes(page);
        const auto result = HeapPage{page}.MarkDead(slot_id);
        EXPECT_FALSE(result);
        EXPECT_EQ(result.error, expected_error);
        EXPECT_EQ(result.page_error, expected_page_error);
        EXPECT_EQ(CopyPageBytes(page), before);
    };

    Page out_of_range = InitializedHeapPage();
    expect_unchanged(out_of_range, 0, HeapPageMarkDeadError::SLOT_OUT_OF_RANGE);
    expect_unchanged(out_of_range, INVALID_SLOT_ID, HeapPageMarkDeadError::SLOT_OUT_OF_RANGE);

    Page corrupt = InitializedHeapPage();
    auto common_header = corrupt.DecodeHeader();
    if (!common_header.has_value()) {
        ADD_FAILURE() << "initialized common header did not decode";
        return;
    }
    common_header->page_type = PageType::FSM_DATA;
    WriteCommonHeader(corrupt, *common_header);
    expect_unchanged(
        corrupt, 0, HeapPageMarkDeadError::PAGE_INVALID, HeapPageValidationError::WRONG_PAGE_TYPE);

    for (const auto state : {HeapSlotState::UNUSED, HeapSlotState::REDIRECT_RESERVED}) {
        Page disallowed = InitializedHeapPage();
        const HeapSlotEntry slot = state == HeapSlotState::UNUSED
                                       ? CanonicalUnusedSlot()
                                       : HeapSlotEntry{.state = HeapSlotState::REDIRECT_RESERVED};
        const std::array slots{slot};
        ConfigureSlotsForValidation(
            disallowed, slots, state == HeapSlotState::UNUSED ? SlotId{0} : INVALID_SLOT_ID);
        ASSERT_TRUE(HeapPage{disallowed}.Validate());
        expect_unchanged(disallowed, 0, HeapPageMarkDeadError::INVALID_SLOT_STATE);
    }

    Page invalid_persisted_state = InitializedHeapPage();
    ConfigureOneNormalSlot(invalid_persisted_state);
    ASSERT_TRUE(
        EncodeLittleEndian(SlotBytes(invalid_persisted_state, 0).subspan(HEAP_SLOT_FLAGS_OFFSET, 2),
                           std::uint16_t{99}));
    expect_unchanged(invalid_persisted_state,
                     0,
                     HeapPageMarkDeadError::PAGE_INVALID,
                     HeapPageValidationError::INVALID_SLOT_STATE);

    Page already_dead = InitializedHeapPage();
    ConfigureOneNormalSlot(already_dead);
    ASSERT_TRUE(HeapPage{already_dead}.MarkDead(0));
    expect_unchanged(already_dead, 0, HeapPageMarkDeadError::ALREADY_DEAD);
}

TEST(HeapPageCompactionTest, ReclaimsDeadPayloadAndPreservesStableRids) {
    Page page = InitializedHeapPage(PageId{.file_id = 51, .page_no = 57});
    HeapPage heap_page{page};
    constexpr std::array first{std::byte{0xA1}, std::byte{0xA2}};
    constexpr std::array second{std::byte{0xB1}, std::byte{0xB2}, std::byte{0xB3}};
    constexpr std::array third{std::byte{0xC1}, std::byte{0xC2}, std::byte{0xC3}, std::byte{0xC4}};
    const auto first_insert = heap_page.Insert(first);
    const auto second_insert = heap_page.Insert(second);
    const auto third_insert = heap_page.Insert(third);
    if (!first_insert.rid.has_value() || !second_insert.rid.has_value() ||
        !third_insert.rid.has_value()) {
        ADD_FAILURE() << "sequential insertion failed";
        return;
    }
    const Rid first_rid = *first_insert.rid;
    const Rid second_rid = *second_insert.rid;
    const Rid third_rid = *third_insert.rid;
    ASSERT_TRUE(heap_page.MarkDead(second_rid.slot));
    auto dead_before_compaction = DecodeHeapSlotEntry(SlotBytes(page, second_rid.slot));
    if (!dead_before_compaction.entry.has_value()) {
        ADD_FAILURE() << "DEAD slot did not decode before compaction";
        return;
    }
    dead_before_compaction.entry->aux = 0xA55A;
    ASSERT_TRUE(
        EncodeHeapSlotEntry(SlotBytes(page, second_rid.slot), *dead_before_compaction.entry));

    const auto original_header = heap_page.Header();
    if (!original_header.has_value()) {
        ADD_FAILURE() << "heap header did not decode before compaction";
        return;
    }
    const auto original_common_header = page.DecodeHeader();
    if (!original_common_header.has_value()) {
        ADD_FAILURE() << "common header did not decode before compaction";
        return;
    }

    const auto result = heap_page.Compact();
    EXPECT_TRUE(result);
    EXPECT_EQ(result.error, HeapPageCompactError::NONE);
    ASSERT_TRUE(heap_page.Validate());

    const auto compacted_header = heap_page.Header();
    if (!compacted_header.has_value()) {
        ADD_FAILURE() << "heap header did not decode after compaction";
        return;
    }
    EXPECT_EQ(compacted_header->slot_count, original_header->slot_count);
    EXPECT_EQ(compacted_header->lower, original_header->lower);
    EXPECT_EQ(compacted_header->upper, original_header->upper + second.size());
    EXPECT_EQ(compacted_header->free_slot_head, original_header->free_slot_head);
    EXPECT_EQ(compacted_header->prune_hint, original_header->prune_hint);
    EXPECT_EQ(compacted_header->reserved, original_header->reserved);
    const auto compacted_common_header = page.DecodeHeader();
    if (!compacted_common_header.has_value()) {
        ADD_FAILURE() << "common header did not decode after compaction";
        return;
    }
    EXPECT_EQ(*compacted_common_header, *original_common_header);

    const auto first_slot = DecodeHeapSlotEntry(SlotBytes(page, first_rid.slot));
    const auto dead_slot = DecodeHeapSlotEntry(SlotBytes(page, second_rid.slot));
    const auto third_slot = DecodeHeapSlotEntry(SlotBytes(page, third_rid.slot));
    if (!first_slot.entry.has_value() || !dead_slot.entry.has_value() ||
        !third_slot.entry.has_value()) {
        ADD_FAILURE() << "compacted slot did not decode";
        return;
    }
    EXPECT_EQ(first_slot.entry->tuple_offset, PAGE_SIZE - first.size());
    EXPECT_EQ(third_slot.entry->tuple_offset, PAGE_SIZE - first.size() - third.size());
    EXPECT_EQ(dead_slot.entry->state, HeapSlotState::DEAD);
    EXPECT_EQ(dead_slot.entry->tuple_offset, std::uint16_t{0});
    EXPECT_EQ(dead_slot.entry->tuple_length, std::uint16_t{0});
    EXPECT_EQ(dead_slot.entry->aux, std::uint16_t{0xA55A});

    const auto stored_first = heap_page.TupleBytes(first_rid.slot);
    const auto stored_third = heap_page.TupleBytes(third_rid.slot);
    if (!stored_first.has_value() || !stored_third.has_value()) {
        ADD_FAILURE() << "live tuple was not retrievable after compaction";
        return;
    }
    EXPECT_TRUE(std::ranges::equal(*stored_first, first));
    EXPECT_FALSE(heap_page.TupleBytes(second_rid.slot).has_value());
    EXPECT_TRUE(std::ranges::equal(*stored_third, third));
    EXPECT_EQ(first_rid, (Rid{.page = page.Id(), .slot = 0}));
    EXPECT_EQ(second_rid, (Rid{.page = page.Id(), .slot = 1}));
    EXPECT_EQ(third_rid, (Rid{.page = page.Id(), .slot = 2}));

    constexpr std::array expected_packed{std::byte{0xC1},
                                         std::byte{0xC2},
                                         std::byte{0xC3},
                                         std::byte{0xC4},
                                         std::byte{0xA1},
                                         std::byte{0xA2}};
    EXPECT_TRUE(std::ranges::equal(
        page.Bytes().subspan(compacted_header->upper, expected_packed.size()), expected_packed));

    const auto once_compacted = CopyPageBytes(page);
    EXPECT_TRUE(heap_page.Compact());
    EXPECT_EQ(CopyPageBytes(page), once_compacted);
}

TEST(HeapPageCompactionTest, HandlesDeadTupleAtEveryPhysicalPositionDeterministically) {
    constexpr std::array payloads{
        std::array{std::byte{0x10}, std::byte{0x11}},
        std::array{std::byte{0x20}, std::byte{0x21}},
        std::array{std::byte{0x30}, std::byte{0x31}},
    };

    for (std::size_t dead_index = 0; dead_index < payloads.size(); ++dead_index) {
        const auto dead_slot_id = static_cast<SlotId>(dead_index);
        Page page = InitializedHeapPage();
        HeapPage heap_page{page};
        for (const auto& payload : payloads) {
            ASSERT_TRUE(heap_page.Insert(payload));
        }
        ASSERT_TRUE(heap_page.MarkDead(dead_slot_id));
        ASSERT_TRUE(heap_page.Compact());

        const auto header = heap_page.Header();
        if (!header.has_value()) {
            ADD_FAILURE() << "heap header did not decode after positional compaction";
            continue;
        }
        EXPECT_EQ(header->upper, PAGE_SIZE - (2 * (payloads.size() - 1)));

        std::size_t expected_offset = PAGE_SIZE;
        for (std::size_t slot_index = 0; slot_index < payloads.size(); ++slot_index) {
            const auto slot_id = static_cast<SlotId>(slot_index);
            const auto slot = DecodeHeapSlotEntry(SlotBytes(page, slot_id));
            if (!slot.entry.has_value()) {
                ADD_FAILURE() << "positional slot did not decode";
                continue;
            }
            if (slot_id == dead_slot_id) {
                EXPECT_EQ(slot.entry->state, HeapSlotState::DEAD);
                EXPECT_EQ(slot.entry->tuple_offset, std::uint16_t{0});
                EXPECT_EQ(slot.entry->tuple_length, std::uint16_t{0});
                EXPECT_FALSE(heap_page.TupleBytes(slot_id).has_value());
                continue;
            }

            expected_offset -= payloads[slot_index].size();
            EXPECT_EQ(slot.entry->state, HeapSlotState::NORMAL);
            EXPECT_EQ(slot.entry->tuple_offset, expected_offset);
            const auto stored = heap_page.TupleBytes(slot_id);
            if (!stored.has_value()) {
                ADD_FAILURE() << "positional live tuple was not retrievable";
                continue;
            }
            EXPECT_TRUE(std::ranges::equal(*stored, payloads[slot_index]));
        }
    }
}

TEST(HeapPageCompactionTest, ReclaimsMultipleDeadPayloadsAndPreservesNormalZeroLengthTuple) {
    Page page = InitializedHeapPage();
    HeapPage heap_page{page};
    constexpr std::array first{
        std::byte{0x10}, std::byte{0x11}, std::byte{0x12}, std::byte{0x13}, std::byte{0x14}};
    constexpr std::array third{std::byte{0x30}, std::byte{0x31}, std::byte{0x32}, std::byte{0x33}};
    constexpr std::array fourth{std::byte{0x40}, std::byte{0x41}, std::byte{0x42}};
    ASSERT_TRUE(heap_page.Insert(first));
    ASSERT_TRUE(heap_page.Insert(std::span<const std::byte>{}));
    ASSERT_TRUE(heap_page.Insert(third));
    ASSERT_TRUE(heap_page.Insert(fourth));
    ASSERT_TRUE(heap_page.MarkDead(0));
    ASSERT_TRUE(heap_page.MarkDead(2));
    const auto before = heap_page.Header();
    if (!before.has_value()) {
        ADD_FAILURE() << "heap header did not decode before multi-DEAD compaction";
        return;
    }

    ASSERT_TRUE(heap_page.Compact());
    const auto after = heap_page.Header();
    if (!after.has_value()) {
        ADD_FAILURE() << "heap header did not decode after multi-DEAD compaction";
        return;
    }
    EXPECT_EQ(after->upper, before->upper + first.size() + third.size());
    EXPECT_EQ(after->upper, PAGE_SIZE - fourth.size());
    EXPECT_FALSE(heap_page.TupleBytes(0).has_value());
    const auto zero_length = heap_page.TupleBytes(1);
    const auto stored_fourth = heap_page.TupleBytes(3);
    if (!zero_length.has_value() || !stored_fourth.has_value()) {
        ADD_FAILURE() << "live tuple was not retrievable after multi-DEAD compaction";
        return;
    }
    EXPECT_TRUE(zero_length->empty());
    EXPECT_TRUE(std::ranges::equal(*stored_fourth, fourth));

    const auto zero_slot = DecodeHeapSlotEntry(SlotBytes(page, 1));
    if (!zero_slot.entry.has_value()) {
        ADD_FAILURE() << "zero-length NORMAL slot did not decode";
        return;
    }
    EXPECT_EQ(zero_slot.entry->state, HeapSlotState::NORMAL);
    EXPECT_EQ(zero_slot.entry->tuple_length, std::uint16_t{0});
    EXPECT_LE(zero_slot.entry->tuple_offset, PAGE_SIZE);
}

TEST(HeapPageCompactionTest, AllDeadRetainsSlotsAndRestoresTupleRegionToPageEnd) {
    Page page = InitializedHeapPage();
    HeapPage heap_page{page};
    constexpr std::array first{std::byte{0x10}, std::byte{0x11}, std::byte{0x12}, std::byte{0x13}};
    constexpr std::array third{std::byte{0x30}, std::byte{0x31}, std::byte{0x32}};
    ASSERT_TRUE(heap_page.Insert(first));
    ASSERT_TRUE(heap_page.Insert(std::span<const std::byte>{}));
    ASSERT_TRUE(heap_page.Insert(third));
    ASSERT_TRUE(heap_page.MarkDead(0));
    ASSERT_TRUE(heap_page.MarkDead(1));
    ASSERT_TRUE(heap_page.MarkDead(2));

    ASSERT_TRUE(heap_page.Compact());
    const auto header = heap_page.Header();
    if (!header.has_value()) {
        ADD_FAILURE() << "all-DEAD heap header did not decode";
        return;
    }
    EXPECT_EQ(header->slot_count, std::uint16_t{3});
    EXPECT_EQ(header->lower, HEAP_PAGE_TOTAL_HEADER_SIZE + (3 * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE));
    EXPECT_EQ(header->upper, std::uint16_t{PAGE_SIZE});
    EXPECT_EQ(header->free_slot_head, INVALID_SLOT_ID);
    for (SlotId slot_id = 0; slot_id < header->slot_count; ++slot_id) {
        const auto slot = DecodeHeapSlotEntry(SlotBytes(page, slot_id));
        if (!slot.entry.has_value()) {
            ADD_FAILURE() << "all-DEAD slot did not decode";
            continue;
        }
        EXPECT_EQ(slot.entry->state, HeapSlotState::DEAD);
        EXPECT_EQ(slot.entry->tuple_offset, std::uint16_t{0});
        EXPECT_EQ(slot.entry->tuple_length, std::uint16_t{0});
        EXPECT_FALSE(heap_page.TupleBytes(slot_id).has_value());
    }
    EXPECT_TRUE(heap_page.Validate());

    const auto compacted = CopyPageBytes(page);
    EXPECT_TRUE(heap_page.Compact());
    EXPECT_EQ(CopyPageBytes(page), compacted);
}

TEST(HeapPageCompactionTest, RejectsUnsafeLayoutsWithoutMutation) {
    const auto expect_unchanged = [](Page& page,
                                     HeapPageCompactError expected_error,
                                     HeapPageValidationError expected_page_error =
                                         HeapPageValidationError::NONE) {
        const auto before = CopyPageBytes(page);
        const auto result = HeapPage{page}.Compact();
        EXPECT_FALSE(result);
        EXPECT_EQ(result.error, expected_error);
        EXPECT_EQ(result.page_error, expected_page_error);
        EXPECT_EQ(CopyPageBytes(page), before);
    };

    Page corrupt_page = InitializedHeapPage();
    auto common_header = corrupt_page.DecodeHeader();
    if (!common_header.has_value()) {
        ADD_FAILURE() << "initialized common header did not decode";
        return;
    }
    common_header->page_type = PageType::FSM_DATA;
    WriteCommonHeader(corrupt_page, *common_header);
    expect_unchanged(
        corrupt_page, HeapPageCompactError::PAGE_INVALID, HeapPageValidationError::WRONG_PAGE_TYPE);

    Page invalid_normal_range = InitializedHeapPage();
    ConfigureOneNormalSlot(invalid_normal_range, PAGE_SIZE - 1, 2);
    expect_unchanged(invalid_normal_range,
                     HeapPageCompactError::PAGE_INVALID,
                     HeapPageValidationError::NORMAL_TUPLE_OUT_OF_BOUNDS);

    Page invalid_dead_range = InitializedHeapPage();
    ConfigureOneNormalSlot(invalid_dead_range);
    ASSERT_TRUE(HeapPage{invalid_dead_range}.MarkDead(0));
    auto dead_slot = DecodeHeapSlotEntry(SlotBytes(invalid_dead_range, 0));
    if (!dead_slot.entry.has_value()) {
        ADD_FAILURE() << "DEAD slot did not decode for range corruption";
        return;
    }
    dead_slot.entry->tuple_offset = 0;
    dead_slot.entry->tuple_length = 1;
    ASSERT_TRUE(EncodeHeapSlotEntry(SlotBytes(invalid_dead_range, 0), *dead_slot.entry));
    ASSERT_TRUE(HeapPage{invalid_dead_range}.Validate());
    expect_unchanged(invalid_dead_range, HeapPageCompactError::TUPLE_RANGE_OUT_OF_BOUNDS);

    Page overlapping = InitializedHeapPage();
    constexpr std::array tuple{std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}};
    ASSERT_TRUE(HeapPage{overlapping}.Insert(tuple));
    ASSERT_TRUE(HeapPage{overlapping}.Insert(tuple));
    auto overlapping_slot = DecodeHeapSlotEntry(SlotBytes(overlapping, 1));
    if (!overlapping_slot.entry.has_value()) {
        ADD_FAILURE() << "overlapping slot did not decode";
        return;
    }
    overlapping_slot.entry->tuple_offset = PAGE_SIZE - tuple.size() - 1;
    ASSERT_TRUE(EncodeHeapSlotEntry(SlotBytes(overlapping, 1), *overlapping_slot.entry));
    ASSERT_TRUE(HeapPage{overlapping}.Validate());
    expect_unchanged(overlapping, HeapPageCompactError::OVERLAPPING_TUPLE_RANGES);

    for (const auto state : {HeapSlotState::UNUSED, HeapSlotState::REDIRECT_RESERVED}) {
        Page unsupported = InitializedHeapPage();
        const HeapSlotEntry slot = state == HeapSlotState::UNUSED
                                       ? CanonicalUnusedSlot()
                                       : HeapSlotEntry{.state = HeapSlotState::REDIRECT_RESERVED};
        const std::array slots{slot};
        ConfigureSlotsForValidation(
            unsupported, slots, state == HeapSlotState::UNUSED ? SlotId{0} : INVALID_SLOT_ID);
        ASSERT_TRUE(HeapPage{unsupported}.Validate());
        expect_unchanged(unsupported, HeapPageCompactError::UNSUPPORTED_SLOT_STATE);
    }

    Page invalid_persisted_state = InitializedHeapPage();
    ConfigureOneNormalSlot(invalid_persisted_state);
    ASSERT_TRUE(
        EncodeLittleEndian(SlotBytes(invalid_persisted_state, 0).subspan(HEAP_SLOT_FLAGS_OFFSET, 2),
                           std::uint16_t{99}));
    expect_unchanged(invalid_persisted_state,
                     HeapPageCompactError::PAGE_INVALID,
                     HeapPageValidationError::INVALID_SLOT_STATE);
}

TEST(HeapPageCompactionTest, ReclaimedSpaceSupportsAppendOnlyInsertionWithoutSlotReuse) {
    Page page = InitializedHeapPage();
    HeapPage heap_page{page};
    std::array<std::byte, 100> retained{};
    std::array<std::byte, 8000> reclaimed{};
    std::array<std::byte, 30> replacement{};
    std::ranges::fill(retained, std::byte{0x11});
    std::ranges::fill(reclaimed, std::byte{0x22});
    std::ranges::fill(replacement, std::byte{0x33});
    ASSERT_TRUE(heap_page.Insert(retained));
    ASSERT_TRUE(heap_page.Insert(reclaimed));
    ASSERT_TRUE(heap_page.MarkDead(1));

    const auto before_failed_insert = CopyPageBytes(page);
    const auto failed_insert = heap_page.Insert(replacement);
    EXPECT_FALSE(failed_insert);
    EXPECT_EQ(failed_insert.error, HeapPageInsertError::INSUFFICIENT_SPACE);
    EXPECT_EQ(CopyPageBytes(page), before_failed_insert);

    ASSERT_TRUE(heap_page.Compact());
    const auto replacement_insert = heap_page.Insert(replacement);
    if (!replacement_insert.rid.has_value()) {
        ADD_FAILURE() << "post-compaction insertion failed";
        return;
    }
    EXPECT_EQ(replacement_insert.rid->slot, SlotId{2});
    const auto dead_slot = DecodeHeapSlotEntry(SlotBytes(page, 1));
    if (!dead_slot.entry.has_value()) {
        ADD_FAILURE() << "DEAD slot did not decode after append-only insertion";
        return;
    }
    EXPECT_EQ(dead_slot.entry->state, HeapSlotState::DEAD);
    EXPECT_EQ(dead_slot.entry->tuple_offset, std::uint16_t{0});
    EXPECT_EQ(dead_slot.entry->tuple_length, std::uint16_t{0});
    const auto header = heap_page.Header();
    if (!header.has_value()) {
        ADD_FAILURE() << "heap header did not decode after append-only insertion";
        return;
    }
    EXPECT_EQ(header->slot_count, std::uint16_t{3});
    const auto stored_replacement = heap_page.TupleBytes(2);
    if (!stored_replacement.has_value()) {
        ADD_FAILURE() << "post-compaction tuple was not retrievable";
        return;
    }
    EXPECT_TRUE(std::ranges::equal(*stored_replacement, replacement));
}

} // namespace
} // namespace dblusblus
