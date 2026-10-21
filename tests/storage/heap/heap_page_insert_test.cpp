#include "common/encoding.h"
#include "storage/heap/heap_page.h"
#include "storage/heap/heap_test_support.h"
#include "storage/heap/heap_test_tuples.h"
#include "storage/page/page.h"
#include "storage/page/page_header.h"
#include "storage/tuple/tuple_codec.h"
#include "storage/tuple/tuple_header.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <vector>

namespace dblusblus::storage {
namespace {

using heap_test::CopyPageBytes;
using heap_test::HeapHeaderBytes;
using heap_test::InitializedHeapPage;
using heap_test::SlotBytes;
using heap_test::WriteCommonHeader;
using heap_test::WriteHeapHeader;

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
    heap_test::WriteTestTuple(page, tuple_offset, tuple_length);
}

using heap_test::MakeTestTuple;

TEST(HeapPageInsertionTest, InsertsValidTupleAndReturnsStableRid) {
    Page page = InitializedHeapPage(PageId{.file_id = FileId{17}, .page_no = 23});
    HeapPage heap_page{page, heap_test::TestSchemaHistory()};
    const auto tuple = MakeTestTuple(64, std::byte{0x7F});

    const auto result = heap_page.Insert(tuple);
    ASSERT_TRUE(result);
    if (!result.Ok()) {
        ADD_FAILURE() << "successful insertion did not return a RID";
        return;
    }
    EXPECT_TRUE(result);
    EXPECT_EQ(*result, (Rid{.page = PageId{.file_id = FileId{17}, .page_no = 23}, .slot = 0}));

    const auto validation = heap_page.Validate();
    ASSERT_TRUE(validation);
    if (!validation.Ok()) {
        ADD_FAILURE() << "validated heap page did not return its header";
        return;
    }
    EXPECT_EQ(validation->heap.slot_count, std::uint16_t{1});
    EXPECT_EQ(validation->heap.free_slot_head, INVALID_SLOT_ID);
    EXPECT_EQ(validation->heap.lower, std::uint16_t{56});
    EXPECT_EQ(validation->heap.upper, static_cast<std::uint16_t>(PAGE_SIZE - tuple.size()));

    const auto slot = DecodeHeapSlotEntry(SlotBytes(page, 0));
    if (!slot.Ok()) {
        ADD_FAILURE() << "inserted slot did not decode";
        return;
    }
    EXPECT_EQ(slot->tuple_offset, PAGE_SIZE - tuple.size());
    EXPECT_EQ(slot->tuple_length, tuple.size());
    EXPECT_EQ(slot->state, HeapSlotState::NORMAL);
    EXPECT_EQ(slot->aux, std::uint16_t{0});

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
    HeapPage heap_page{page, heap_test::TestSchemaHistory()};
    const auto first = MakeTestTuple(60, std::byte{0x10});
    const auto second = MakeTestTuple(61, std::byte{0x20});
    const auto third = MakeTestTuple(heap_test::TEST_TUPLE_MIN_SIZE);

    const auto first_result = heap_page.Insert(first);
    const auto second_result = heap_page.Insert(second);
    const auto third_result = heap_page.Insert(third);
    if (!first_result.Ok() || !second_result.Ok() || !third_result.Ok()) {
        ADD_FAILURE() << "one of the sequential insertions failed";
        return;
    }
    EXPECT_EQ(first_result->slot, SlotId{0});
    EXPECT_EQ(second_result->slot, SlotId{1});
    EXPECT_EQ(third_result->slot, SlotId{2});

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
    if (!first_slot.Ok() || !second_slot.Ok() || !third_slot.Ok()) {
        ADD_FAILURE() << "one of the sequential slots did not decode";
        return;
    }
    EXPECT_EQ(first_slot->tuple_offset, PAGE_SIZE - first.size());
    EXPECT_EQ(second_slot->tuple_offset, PAGE_SIZE - first.size() - second.size());
    EXPECT_EQ(third_slot->tuple_offset, PAGE_SIZE - first.size() - second.size() - third.size());

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
    HeapPage heap_page{page, heap_test::TestSchemaHistory()};
    const auto maximum_tuple = MakeTestTuple(HEAP_PAGE_MAX_RAW_TUPLE_SIZE);

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
    const auto no_space = heap_page.Insert(MakeTestTuple(heap_test::TEST_TUPLE_MIN_SIZE));
    EXPECT_FALSE(no_space);
    EXPECT_EQ(no_space.Error().code, HeapPageInsertError::INSUFFICIENT_SPACE);
    EXPECT_EQ(CopyPageBytes(page), before_no_space);

    Page oversized_page = InitializedHeapPage();
    HeapPage oversized_heap_page{oversized_page, heap_test::TestSchemaHistory()};
    std::array<std::byte, HEAP_PAGE_MAX_RAW_TUPLE_SIZE + 1> oversized_tuple{};
    const auto before_oversized = CopyPageBytes(oversized_page);
    const auto oversized = oversized_heap_page.Insert(oversized_tuple);
    EXPECT_FALSE(oversized);
    EXPECT_EQ(oversized.Error().code, HeapPageInsertError::TUPLE_TOO_LARGE);
    EXPECT_EQ(CopyPageBytes(oversized_page), before_oversized);
}

TEST(HeapPageInsertionTest, RejectsTuplesThatWouldFailOwnerValidationWithoutMutation) {
    const auto expect_rejected = [](Page& page,
                                    std::span<const std::byte> tuple,
                                    HeapPageValidationError expected_tuple_error) {
        const auto before = CopyPageBytes(page);
        const auto result = heap_test::TestHeapPage(page).Insert(tuple);
        EXPECT_FALSE(result);
        EXPECT_EQ(result.Error().code, HeapPageInsertError::INVALID_TUPLE);
        EXPECT_EQ(result.Error().page_error, HeapPageValidationError::NONE);
        EXPECT_EQ(result.Error().tuple_error, expected_tuple_error);
        EXPECT_EQ(CopyPageBytes(page), before);
    };

    // The page under test is page 3, and the first insertion would occupy slot 0.
    Page page = InitializedHeapPage({.file_id = FileId{7}, .page_no = 3});
    expect_rejected(page, {}, HeapPageValidationError::INVALID_TUPLE);
    const std::vector<std::byte> garbage(64, std::byte{0x42});
    expect_rejected(page, garbage, HeapPageValidationError::INVALID_TUPLE);

    auto unknown_schema = MakeTestTuple(64);
    ASSERT_TRUE(EncodeLittleEndian(
        std::span{unknown_schema}.subspan(TUPLE_HEADER_SCHEMA_VERSION_OFFSET, sizeof(SchemaVer)),
        SchemaVer{9}));
    expect_rejected(page, unknown_schema, HeapPageValidationError::UNRESOLVED_SCHEMA_VERSION);

    expect_rejected(page,
                    MakeTestTuple(64, std::byte{0x5A}, TupleVersionMetadata{}),
                    HeapPageValidationError::INVALID_TUPLE_TRANSACTION_FIELDS);

    auto self_reference = heap_test::TestTupleMetadata();
    self_reference.prev_page_no = 3;
    self_reference.prev_slot = 0;
    expect_rejected(page,
                    MakeTestTuple(64, std::byte{0x5A}, self_reference),
                    HeapPageValidationError::INVALID_PREVIOUS_VERSION_POINTER);

    auto beyond_new_slot = self_reference;
    beyond_new_slot.prev_slot = 1;
    expect_rejected(page,
                    MakeTestTuple(64, std::byte{0x5A}, beyond_new_slot),
                    HeapPageValidationError::INVALID_PREVIOUS_VERSION_POINTER);
}

TEST(HeapPageInsertionTest, AcceptsSamePagePreviousVersionOfExistingSlot) {
    Page page = InitializedHeapPage({.file_id = FileId{7}, .page_no = 3});
    HeapPage heap_page{page, heap_test::TestSchemaHistory()};
    ASSERT_TRUE(heap_page.Insert(MakeTestTuple(64)));

    auto metadata = heap_test::TestTupleMetadata();
    metadata.prev_page_no = 3;
    metadata.prev_slot = 0;
    const auto result = heap_page.Insert(MakeTestTuple(64, std::byte{0x5A}, metadata));
    if (!result.Ok()) {
        ADD_FAILURE() << "same-page successor insertion failed";
        return;
    }
    EXPECT_EQ(result->slot, SlotId{1});
    EXPECT_TRUE(heap_page.Validate());
}

TEST(HeapPageInsertionTest, RejectsCorruptPagesWithoutChangingBytes) {
    const auto expect_invalid_unchanged = [](Page& page, HeapPageValidationError expected_error) {
        const auto before = CopyPageBytes(page);
        const auto tuple = MakeTestTuple(60);
        const auto result = heap_test::TestHeapPage(page).Insert(tuple);
        EXPECT_FALSE(result);
        EXPECT_EQ(result.Error().code, HeapPageInsertError::PAGE_INVALID);
        EXPECT_EQ(result.Error().page_error, expected_error);
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
    HeapPage heap_page{page, heap_test::TestSchemaHistory()};
    ASSERT_TRUE(heap_page.Insert(MakeTestTuple(58)));

    EXPECT_FALSE(heap_page.TupleBytes(1).has_value());
    auto slot = DecodeHeapSlotEntry(SlotBytes(page, 0));
    if (!slot.Ok()) {
        ADD_FAILURE() << "inserted slot did not decode";
        return;
    }
    slot->state = HeapSlotState::DEAD;
    ASSERT_TRUE(EncodeHeapSlotEntry(SlotBytes(page, 0), *slot));
    EXPECT_TRUE(heap_page.Validate());
    EXPECT_FALSE(heap_page.TupleBytes(0).has_value());
}

} // namespace
} // namespace dblusblus::storage
