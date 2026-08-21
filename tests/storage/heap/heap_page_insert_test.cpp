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

TEST(HeapPageInsertionTest, InsertsOpaqueBytesAndReturnsStableRid) {
    Page page = InitializedHeapPage(PageId{.file_id = 17, .page_no = 23});
    HeapPage heap_page{page};
    constexpr std::array tuple{std::byte{0x00}, std::byte{0x7F}, std::byte{0x80}, std::byte{0xFF}};

    const auto result = heap_page.Insert(tuple);
    ASSERT_TRUE(result);
    if (!result.rid.has_value()) {
        ADD_FAILURE() << "successful insertion did not return a RID";
        return;
    }
    EXPECT_EQ(result.error, HeapPageInsertError::NONE);
    EXPECT_EQ(*result.rid, (Rid{.page = PageId{.file_id = 17, .page_no = 23}, .slot = 0}));

    const auto validation = heap_page.Validate();
    ASSERT_TRUE(validation);
    if (!validation.heap_header.has_value()) {
        ADD_FAILURE() << "validated heap page did not return its header";
        return;
    }
    EXPECT_EQ(validation.heap_header->slot_count, std::uint16_t{1});
    EXPECT_EQ(validation.heap_header->free_slot_head, INVALID_SLOT_ID);
    EXPECT_EQ(validation.heap_header->lower, std::uint16_t{56});
    EXPECT_EQ(validation.heap_header->upper, std::uint16_t{PAGE_SIZE - tuple.size()});

    const auto slot = DecodeHeapSlotEntry(SlotBytes(page, 0));
    if (!slot.entry.has_value()) {
        ADD_FAILURE() << "inserted slot did not decode";
        return;
    }
    EXPECT_EQ(slot.entry->tuple_offset, PAGE_SIZE - tuple.size());
    EXPECT_EQ(slot.entry->tuple_length, tuple.size());
    EXPECT_EQ(slot.entry->state, HeapSlotState::NORMAL);
    EXPECT_EQ(slot.entry->aux, std::uint16_t{0});

    const auto stored = heap_page.TupleBytes(0);
    if (!stored.has_value()) {
        ADD_FAILURE() << "inserted tuple was not retrievable";
        return;
    }
    EXPECT_TRUE(std::ranges::equal(*stored, tuple));
    const auto common_header = page.DecodeHeader();
    if (!common_header.has_value()) {
        ADD_FAILURE() << "common header did not decode after insertion";
        return;
    }
    EXPECT_EQ(common_header->checksum_crc32c, std::uint32_t{0});
}

TEST(HeapPageInsertionTest, MultipleInsertsGrowTowardEachOtherWithoutMovingPriorTuples) {
    Page page = InitializedHeapPage();
    HeapPage heap_page{page};
    constexpr std::array first{std::byte{0x10}, std::byte{0x11}};
    constexpr std::array second{std::byte{0x20}, std::byte{0x21}, std::byte{0x22}};
    constexpr std::array third{std::byte{0x30}};

    const auto first_result = heap_page.Insert(first);
    const auto second_result = heap_page.Insert(second);
    const auto third_result = heap_page.Insert(third);
    if (!first_result.rid.has_value() || !second_result.rid.has_value() ||
        !third_result.rid.has_value()) {
        ADD_FAILURE() << "one of the sequential insertions failed";
        return;
    }
    EXPECT_EQ(first_result.rid->slot, SlotId{0});
    EXPECT_EQ(second_result.rid->slot, SlotId{1});
    EXPECT_EQ(third_result.rid->slot, SlotId{2});

    const auto header = heap_page.Header();
    if (!header.has_value()) {
        ADD_FAILURE() << "heap header did not decode after sequential insertions";
        return;
    }
    EXPECT_EQ(header->slot_count, std::uint16_t{3});
    EXPECT_EQ(header->lower, HEAP_PAGE_TOTAL_HEADER_SIZE + (3 * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE));
    EXPECT_EQ(header->upper, PAGE_SIZE - first.size() - second.size() - third.size());

    const auto first_slot = DecodeHeapSlotEntry(SlotBytes(page, 0));
    const auto second_slot = DecodeHeapSlotEntry(SlotBytes(page, 1));
    const auto third_slot = DecodeHeapSlotEntry(SlotBytes(page, 2));
    if (!first_slot.entry.has_value() || !second_slot.entry.has_value() ||
        !third_slot.entry.has_value()) {
        ADD_FAILURE() << "one of the sequential slots did not decode";
        return;
    }
    EXPECT_EQ(first_slot.entry->tuple_offset, PAGE_SIZE - first.size());
    EXPECT_EQ(second_slot.entry->tuple_offset, PAGE_SIZE - first.size() - second.size());
    EXPECT_EQ(third_slot.entry->tuple_offset,
              PAGE_SIZE - first.size() - second.size() - third.size());

    const auto stored_first = heap_page.TupleBytes(0);
    const auto stored_second = heap_page.TupleBytes(1);
    const auto stored_third = heap_page.TupleBytes(2);
    if (!stored_first.has_value() || !stored_second.has_value() || !stored_third.has_value()) {
        ADD_FAILURE() << "one of the sequential tuples was not retrievable";
        return;
    }
    EXPECT_TRUE(std::ranges::equal(*stored_first, first));
    EXPECT_TRUE(std::ranges::equal(*stored_second, second));
    EXPECT_TRUE(std::ranges::equal(*stored_third, third));
}

TEST(HeapPageInsertionTest, EnforcesInlineLimitAndContiguousFreeSpaceAtomically) {
    Page page = InitializedHeapPage();
    HeapPage heap_page{page};
    std::array<std::byte, HEAP_PAGE_MAX_RAW_TUPLE_SIZE> maximum_tuple{};
    std::ranges::fill(maximum_tuple, std::byte{0x5A});

    const auto maximum_result = heap_page.Insert(maximum_tuple);
    ASSERT_TRUE(maximum_result);
    const auto header = heap_page.Header();
    if (!header.has_value()) {
        ADD_FAILURE() << "heap header did not decode after maximum insertion";
        return;
    }
    EXPECT_EQ(header->lower, std::uint16_t{56});
    EXPECT_EQ(header->upper, std::uint16_t{57});

    const auto before_no_space = CopyPageBytes(page);
    const auto no_space = heap_page.Insert(std::span<const std::byte>{});
    EXPECT_FALSE(no_space);
    EXPECT_EQ(no_space.error, HeapPageInsertError::INSUFFICIENT_SPACE);
    EXPECT_EQ(CopyPageBytes(page), before_no_space);

    Page oversized_page = InitializedHeapPage();
    HeapPage oversized_heap_page{oversized_page};
    std::array<std::byte, HEAP_PAGE_MAX_RAW_TUPLE_SIZE + 1> oversized_tuple{};
    const auto before_oversized = CopyPageBytes(oversized_page);
    const auto oversized = oversized_heap_page.Insert(oversized_tuple);
    EXPECT_FALSE(oversized);
    EXPECT_EQ(oversized.error, HeapPageInsertError::TUPLE_TOO_LARGE);
    EXPECT_EQ(CopyPageBytes(oversized_page), before_oversized);
}

TEST(HeapPageInsertionTest, AcceptsZeroLengthOpaqueTuple) {
    Page page = InitializedHeapPage();
    HeapPage heap_page{page};

    const auto result = heap_page.Insert(std::span<const std::byte>{});
    if (!result.rid.has_value()) {
        ADD_FAILURE() << "zero-length tuple insertion did not return a RID";
        return;
    }
    EXPECT_EQ(result.rid->slot, SlotId{0});
    const auto header = heap_page.Header();
    if (!header.has_value()) {
        ADD_FAILURE() << "heap header did not decode after zero-length insertion";
        return;
    }
    EXPECT_EQ(header->slot_count, std::uint16_t{1});
    EXPECT_EQ(header->lower, std::uint16_t{56});
    EXPECT_EQ(header->upper, std::uint16_t{PAGE_SIZE});

    const auto slot = DecodeHeapSlotEntry(SlotBytes(page, 0));
    if (!slot.entry.has_value()) {
        ADD_FAILURE() << "zero-length tuple slot did not decode";
        return;
    }
    EXPECT_EQ(slot.entry->tuple_offset, std::uint16_t{PAGE_SIZE});
    EXPECT_EQ(slot.entry->tuple_length, std::uint16_t{0});
    EXPECT_EQ(slot.entry->state, HeapSlotState::NORMAL);
    EXPECT_EQ(slot.entry->aux, std::uint16_t{0});
    const auto stored = heap_page.TupleBytes(0);
    if (!stored.has_value()) {
        ADD_FAILURE() << "zero-length tuple was not retrievable";
        return;
    }
    EXPECT_TRUE(stored->empty());
}

TEST(HeapPageInsertionTest, RejectsCorruptPagesWithoutChangingBytes) {
    const auto expect_invalid_unchanged = [](Page& page, HeapPageValidationError expected_error) {
        const auto before = CopyPageBytes(page);
        constexpr std::array tuple{std::byte{0x42}};
        const auto result = HeapPage{page}.Insert(tuple);
        EXPECT_FALSE(result);
        EXPECT_EQ(result.error, HeapPageInsertError::PAGE_INVALID);
        EXPECT_EQ(result.page_error, expected_error);
        EXPECT_EQ(CopyPageBytes(page), before);
    };

    Page wrong_type = InitializedHeapPage();
    auto common_header = wrong_type.DecodeHeader();
    if (!common_header.has_value()) {
        ADD_FAILURE() << "initialized common header did not decode";
        return;
    }
    common_header->page_type = PageType::FSM_DATA;
    WriteCommonHeader(wrong_type, *common_header);
    expect_invalid_unchanged(wrong_type, HeapPageValidationError::WRONG_PAGE_TYPE);

    Page bad_geometry = InitializedHeapPage();
    auto heap_header = HeapPageHeader{};
    heap_header.lower = 100;
    heap_header.upper = 99;
    WriteHeapHeader(bad_geometry, heap_header);
    expect_invalid_unchanged(bad_geometry, HeapPageValidationError::LOWER_AFTER_UPPER);

    Page inconsistent_slots = InitializedHeapPage();
    heap_header = HeapPageHeader{};
    heap_header.slot_count = 1;
    WriteHeapHeader(inconsistent_slots, heap_header);
    expect_invalid_unchanged(inconsistent_slots,
                             HeapPageValidationError::SLOT_COUNT_LOWER_MISMATCH);

    Page invalid_slot = InitializedHeapPage();
    ConfigureOneNormalSlot(invalid_slot);
    ASSERT_TRUE(EncodeLittleEndian(SlotBytes(invalid_slot, 0).subspan(HEAP_SLOT_FLAGS_OFFSET, 2),
                                   std::uint16_t{99}));
    expect_invalid_unchanged(invalid_slot, HeapPageValidationError::INVALID_SLOT_STATE);
}

TEST(HeapPageInsertionTest, TupleBytesRejectsInvalidAndNonNormalSlots) {
    Page page = InitializedHeapPage();
    HeapPage heap_page{page};
    constexpr std::array tuple{std::byte{0x31}, std::byte{0x32}};
    ASSERT_TRUE(heap_page.Insert(tuple));

    EXPECT_FALSE(heap_page.TupleBytes(1).has_value());
    auto slot = DecodeHeapSlotEntry(SlotBytes(page, 0));
    if (!slot.entry.has_value()) {
        ADD_FAILURE() << "inserted slot did not decode";
        return;
    }
    slot.entry->state = HeapSlotState::DEAD;
    ASSERT_TRUE(EncodeHeapSlotEntry(SlotBytes(page, 0), *slot.entry));
    EXPECT_TRUE(heap_page.Validate());
    EXPECT_FALSE(heap_page.TupleBytes(0).has_value());
}

} // namespace
} // namespace dblusblus
