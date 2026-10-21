#include "common/encoding.h"
#include "storage/heap/heap_page.h"
#include "storage/heap/heap_test_tuples.h"
#include "storage/page/page.h"
#include "storage/page/page_header.h"
#include "storage/tuple/tuple_codec.h"
#include "storage/tuple/tuple_header.h"
#include "storage/tuple/tuple_layout.h"

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
    HeapPage heap_page{page, heap_test::TestSchemaHistory()};
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
    const std::size_t tuple_end = std::size_t{tuple_offset} + tuple_length;
    if (tuple_length >= heap_test::TEST_TUPLE_MIN_SIZE && tuple_end <= PAGE_SIZE) {
        heap_test::WriteTestTuple(page, tuple_offset, tuple_length);
    }
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

// ConfigureSlotsForValidation packs a real tuple of TEST_TUPLE_MIN_SIZE bytes for this slot.
[[nodiscard]] constexpr HeapSlotEntry PackedNormalSlot() noexcept {
    return HeapSlotEntry{
        .tuple_offset = 0,
        .tuple_length = 0,
        .state = HeapSlotState::NORMAL,
        .aux = 0,
    };
}

[[nodiscard]] constexpr HeapSlotEntry CanonicalDeadSlot() noexcept {
    return HeapSlotEntry{
        .tuple_offset = 0,
        .tuple_length = 0,
        .state = HeapSlotState::DEAD,
        .aux = 0,
    };
}

// NORMAL slots given as PackedNormalSlot() receive valid tuples packed down from the page end;
// every other slot is written exactly as given.
void ConfigureSlotsForValidation(Page& page,
                                 std::span<const HeapSlotEntry> slots,
                                 SlotId free_slot_head = INVALID_SLOT_ID) {
    constexpr std::size_t maximum_slot_count =
        (PAGE_SIZE - HEAP_PAGE_SLOT_DIRECTORY_OFFSET) / HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE;
    ASSERT_LE(slots.size(), maximum_slot_count);

    std::size_t upper = PAGE_SIZE;
    for (std::size_t index = 0; index < slots.size(); ++index) {
        HeapSlotEntry slot = slots[index];
        if (slot == PackedNormalSlot()) {
            upper -= heap_test::TEST_TUPLE_MIN_SIZE;
            slot.tuple_offset = static_cast<std::uint16_t>(upper);
            slot.tuple_length = static_cast<std::uint16_t>(heap_test::TEST_TUPLE_MIN_SIZE);
            heap_test::WriteTestTuple(page, upper, heap_test::TEST_TUPLE_MIN_SIZE);
        }
        ASSERT_TRUE(EncodeHeapSlotEntry(SlotBytes(page, static_cast<SlotId>(index)), slot));
    }

    const auto slot_count = static_cast<std::uint16_t>(slots.size());
    const auto lower = static_cast<std::uint16_t>(
        HEAP_PAGE_SLOT_DIRECTORY_OFFSET + (slots.size() * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE));
    WriteHeapHeader(page,
                    HeapPageHeader{
                        .slot_count = slot_count,
                        .free_slot_head = free_slot_head,
                        .lower = lower,
                        .upper = static_cast<std::uint16_t>(upper),
                    });
}

// Writes `slots` verbatim with a fixed tuple-region boundary.
void WriteSlots(Page& page, std::span<const HeapSlotEntry> slots, std::uint16_t upper) {
    WriteHeapHeader(
        page,
        HeapPageHeader{
            .slot_count = static_cast<std::uint16_t>(slots.size()),
            .free_slot_head = INVALID_SLOT_ID,
            .lower = static_cast<std::uint16_t>(HEAP_PAGE_SLOT_DIRECTORY_OFFSET +
                                                (slots.size() * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE)),
            .upper = upper,
        });
    for (std::size_t index = 0; index < slots.size(); ++index) {
        ASSERT_TRUE(EncodeHeapSlotEntry(SlotBytes(page, static_cast<SlotId>(index)), slots[index]));
    }
}

[[nodiscard]] constexpr HeapSlotEntry
RetainedSlot(HeapSlotState state, std::uint16_t offset, std::uint16_t length) noexcept {
    return HeapSlotEntry{
        .tuple_offset = offset,
        .tuple_length = length,
        .state = state,
        .aux = 0,
    };
}

TEST(HeapPageTest, InitializesDeterministicBlankHeapDataPage) {
    Page page{PageId{.file_id = 17, .page_no = 23}};
    std::ranges::fill(page.Bytes(), std::byte{0xA5});
    HeapPage heap_page{page, heap_test::TestSchemaHistory()};

    ASSERT_TRUE(heap_page.Initialize());
    const auto validation = heap_page.Validate();
    ASSERT_TRUE(validation);
    if (!validation.common_header.has_value() || !validation.heap_header.has_value()) {
        ADD_FAILURE() << "validated blank heap page did not return headers";
        return;
    }

    EXPECT_EQ(validation.common_header->page_type, PageType::HEAP_DATA);
    EXPECT_EQ(validation.common_header->format_version, HEAP_PAGE_FORMAT_VERSION);
    EXPECT_EQ(validation.common_header->flags, std::uint32_t{0});
    EXPECT_EQ(validation.common_header->page_lsn, INVALID_LSN);
    EXPECT_EQ(validation.common_header->checksum_crc32c, std::uint32_t{0});
    EXPECT_EQ(validation.common_header->header_size, HEAP_PAGE_TOTAL_HEADER_SIZE);
    EXPECT_EQ(validation.common_header->reserved16, std::uint16_t{0});
    EXPECT_EQ(validation.common_header->page_no, PageNo{23});

    EXPECT_EQ(validation.heap_header->slot_count, std::uint16_t{0});
    EXPECT_EQ(validation.heap_header->free_slot_head, INVALID_SLOT_ID);
    EXPECT_EQ(validation.heap_header->lower, HEAP_PAGE_TOTAL_HEADER_SIZE);
    EXPECT_EQ(validation.heap_header->upper, PAGE_SIZE);
    EXPECT_EQ(validation.heap_header->prune_hint, std::uint32_t{0});
    EXPECT_EQ(validation.heap_header->reserved, std::uint32_t{0});

    EXPECT_EQ(page.Bytes()[HEAP_PAGE_SLOT_COUNT_OFFSET], std::byte{0x00});
    EXPECT_EQ(page.Bytes()[HEAP_PAGE_SLOT_COUNT_OFFSET + 1], std::byte{0x00});
    EXPECT_EQ(page.Bytes()[HEAP_PAGE_FREE_SLOT_HEAD_OFFSET], std::byte{0xFF});
    EXPECT_EQ(page.Bytes()[HEAP_PAGE_FREE_SLOT_HEAD_OFFSET + 1], std::byte{0xFF});
    EXPECT_EQ(page.Bytes()[HEAP_PAGE_LOWER_OFFSET], std::byte{0x30});
    EXPECT_EQ(page.Bytes()[HEAP_PAGE_LOWER_OFFSET + 1], std::byte{0x00});
    EXPECT_EQ(page.Bytes()[HEAP_PAGE_UPPER_OFFSET], std::byte{0x00});
    EXPECT_EQ(page.Bytes()[HEAP_PAGE_UPPER_OFFSET + 1], std::byte{0x20});
    EXPECT_TRUE(std::ranges::all_of(
        page.Bytes().subspan(HEAP_PAGE_PRUNE_HINT_OFFSET, sizeof(std::uint32_t) * 2),
        [](std::byte value) { return value == std::byte{0}; }));
    EXPECT_TRUE(std::ranges::all_of(page.Bytes().subspan(HEAP_PAGE_TOTAL_HEADER_SIZE),
                                    [](std::byte value) { return value == std::byte{0}; }));
}

TEST(HeapPageTest, InitializesLowestOrdinaryPageNumber) {
    Page page{PageId{.file_id = 18, .page_no = 1}};
    HeapPage heap_page{page, heap_test::TestSchemaHistory()};

    ASSERT_TRUE(heap_page.Initialize());
    const auto validation = heap_page.Validate();
    ASSERT_TRUE(validation);
    if (!validation.common_header.has_value()) {
        ADD_FAILURE() << "validated page did not return its common header";
        return;
    }
    EXPECT_EQ(validation.common_header->page_no, PageNo{1});
}

TEST(HeapPageTest, InitializationRejectsNonOrdinaryPageNumbersWithoutMutation) {
    constexpr std::array invalid_page_numbers{PageNo{0}, INVALID_PAGE_NO};

    for (const PageNo page_no : invalid_page_numbers) {
        Page page{PageId{.file_id = 19, .page_no = page_no}};
        std::ranges::fill(page.Bytes(), std::byte{0xA5});
        const auto original = CopyPageBytes(page);

        EXPECT_FALSE(heap_test::TestHeapPage(page).Initialize());
        EXPECT_TRUE(std::ranges::equal(page.Bytes(), original));
    }
}

TEST(HeapPageTest, InitializationWritesZeroFlagsAndPreservesExplicitPageLsn) {
    Page page{PageId{.file_id = 18, .page_no = 24}};
    HeapPage heap_page{page, heap_test::TestSchemaHistory()};
    ASSERT_TRUE(heap_page.Initialize(Lsn{77}));

    const auto common_header = page.DecodeHeader();
    if (!common_header.has_value()) {
        ADD_FAILURE() << "initialized common header decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(common_header->flags, std::uint32_t{0});
    EXPECT_EQ(common_header->page_lsn, Lsn{77});
    EXPECT_EQ(common_header->checksum_crc32c, std::uint32_t{0});
}

TEST(HeapPageValidationTest, AcceptsCanonicalPageWithoutUnusedSlots) {
    Page page = InitializedHeapPage();
    ConfigureOneNormalSlot(page);

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_TRUE(result);
}

TEST(HeapPageValidationTest, RejectsWrongCommonHeaderIdentityAndFormat) {
    Page page = InitializedHeapPage();
    const auto original = page.DecodeHeader();
    if (!original.has_value()) {
        ADD_FAILURE() << "initialized common header decode unexpectedly failed";
        return;
    }
    const CommonPageHeader original_header = *original;

    auto common_header = original_header;
    common_header.page_type = PageType::FSM_DATA;
    WriteCommonHeader(page, common_header);
    EXPECT_EQ(heap_test::TestHeapPage(page).Validate().error,
              HeapPageValidationError::WRONG_PAGE_TYPE);

    common_header = original_header;
    common_header.page_no += 1;
    WriteCommonHeader(page, common_header);
    EXPECT_EQ(heap_test::TestHeapPage(page).Validate().error,
              HeapPageValidationError::WRONG_PAGE_NUMBER);

    common_header = original_header;
    common_header.header_size = COMMON_PAGE_HEADER_ENCODED_SIZE;
    WriteCommonHeader(page, common_header);
    EXPECT_EQ(heap_test::TestHeapPage(page).Validate().error,
              HeapPageValidationError::WRONG_HEADER_SIZE);

    common_header = original_header;
    common_header.format_version = HEAP_PAGE_FORMAT_VERSION + 1;
    WriteCommonHeader(page, common_header);
    EXPECT_EQ(heap_test::TestHeapPage(page).Validate().error,
              HeapPageValidationError::UNSUPPORTED_FORMAT_VERSION);

    common_header = original_header;
    common_header.reserved16 = 1;
    WriteCommonHeader(page, common_header);
    EXPECT_EQ(heap_test::TestHeapPage(page).Validate().error,
              HeapPageValidationError::NONZERO_COMMON_RESERVED);
}

TEST(HeapPageValidationTest, RejectsNonOrdinaryOwningPageNumbers) {
    const Page canonical = InitializedHeapPage({.file_id = 20, .page_no = 1});
    constexpr std::array invalid_page_numbers{PageNo{0}, INVALID_PAGE_NO};

    for (const PageNo page_no : invalid_page_numbers) {
        Page page{PageId{.file_id = 20, .page_no = page_no}};
        std::ranges::copy(canonical.Bytes(), page.Bytes().begin());
        auto common_header = page.DecodeHeader();
        if (!common_header.has_value()) {
            ADD_FAILURE() << "canonical common header did not decode";
            return;
        }
        CommonPageHeader updated_header = *common_header;
        updated_header.page_no = page_no;
        WriteCommonHeader(page, updated_header);

        EXPECT_EQ(heap_test::TestHeapPage(page).Validate().error,
                  HeapPageValidationError::INVALID_PAGE_NUMBER);
    }
}

TEST(HeapPageValidationTest, RejectsNonzeroCommonFlags) {
    constexpr std::array invalid_flags{std::uint32_t{0x00000001U}, std::uint32_t{0xFFFFFFFFU}};

    for (const auto flags : invalid_flags) {
        Page page = InitializedHeapPage();
        const auto decoded_header = page.DecodeHeader();
        if (!decoded_header.has_value()) {
            ADD_FAILURE() << "initialized common header decode unexpectedly failed";
            return;
        }
        auto common_header = *decoded_header;
        common_header.flags = flags;
        WriteCommonHeader(page, common_header);

        EXPECT_EQ(heap_test::TestHeapPage(page).Validate().error,
                  HeapPageValidationError::NONZERO_COMMON_FLAGS);
    }
}

TEST(HeapPageValidationTest, RejectsInvalidHeapHeaderGeometryAndReservedField) {
    Page page = InitializedHeapPage();
    auto header = HeapPageHeader{};

    header.lower = HEAP_PAGE_TOTAL_HEADER_SIZE - 1;
    WriteHeapHeader(page, header);
    EXPECT_EQ(heap_test::TestHeapPage(page).Validate().error,
              HeapPageValidationError::LOWER_BEFORE_HEADER);

    header = HeapPageHeader{};
    header.upper = PAGE_SIZE + 1;
    WriteHeapHeader(page, header);
    EXPECT_EQ(heap_test::TestHeapPage(page).Validate().error,
              HeapPageValidationError::UPPER_AFTER_PAGE);

    header = HeapPageHeader{};
    header.lower = 100;
    header.upper = 99;
    WriteHeapHeader(page, header);
    EXPECT_EQ(heap_test::TestHeapPage(page).Validate().error,
              HeapPageValidationError::LOWER_AFTER_UPPER);

    header = HeapPageHeader{};
    header.slot_count = std::numeric_limits<std::uint16_t>::max();
    WriteHeapHeader(page, header);
    EXPECT_EQ(heap_test::TestHeapPage(page).Validate().error,
              HeapPageValidationError::SLOT_DIRECTORY_OUT_OF_BOUNDS);

    header = HeapPageHeader{};
    header.slot_count = 1;
    WriteHeapHeader(page, header);
    EXPECT_EQ(heap_test::TestHeapPage(page).Validate().error,
              HeapPageValidationError::SLOT_COUNT_LOWER_MISMATCH);

    header = HeapPageHeader{};
    header.free_slot_head = 0;
    WriteHeapHeader(page, header);
    EXPECT_EQ(heap_test::TestHeapPage(page).Validate().error,
              HeapPageValidationError::INVALID_FREE_SLOT_HEAD);

    header = HeapPageHeader{};
    header.reserved = 1;
    WriteHeapHeader(page, header);
    EXPECT_EQ(heap_test::TestHeapPage(page).Validate().error,
              HeapPageValidationError::NONZERO_HEAP_RESERVED);
}

TEST(HeapPageValidationTest, RejectsInvalidSlotState) {
    Page page = InitializedHeapPage();
    ConfigureOneNormalSlot(page);
    ASSERT_TRUE(EncodeLittleEndian(SlotBytes(page, 0).subspan(HEAP_SLOT_FLAGS_OFFSET, 2),
                                   std::uint16_t{99}));

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_SLOT_STATE);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsNormalTupleRangesOutsideTupleRegionOrPage) {
    Page page = InitializedHeapPage();

    ConfigureOneNormalSlot(page, 7999, 1);
    auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::RETAINED_TUPLE_OUT_OF_BOUNDS);
    EXPECT_EQ(result.slot_id, SlotId{0});

    ConfigureOneNormalSlot(page, 8180, 20);
    result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::RETAINED_TUPLE_OUT_OF_BOUNDS);

    ConfigureOneNormalSlot(page, std::numeric_limits<std::uint16_t>::max() - 5U, 10);
    result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::RETAINED_TUPLE_OUT_OF_BOUNDS);
}

TEST(HeapPageValidationTest, AcceptsCanonicalSingleElementFreeListWithSentinelTermination) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalUnusedSlot()};
    ConfigureSlotsForValidation(page, slots, 0);

    EXPECT_TRUE(heap_test::TestHeapPage(page).Validate());
}

TEST(HeapPageValidationTest, AcceptsCanonicalMultiElementFreeList) {
    Page page = InitializedHeapPage();
    const std::array slots{
        CanonicalUnusedSlot(2),
        CanonicalUnusedSlot(),
        CanonicalUnusedSlot(1),
    };
    ConfigureSlotsForValidation(page, slots, 0);

    EXPECT_TRUE(heap_test::TestHeapPage(page).Validate());
}

TEST(HeapPageValidationTest, RejectsUnusedSlotWithNoncanonicalTupleOffset) {
    Page page = InitializedHeapPage();
    auto unused_slot = CanonicalUnusedSlot();
    unused_slot.tuple_offset = 1;
    const std::array slots{unused_slot};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::NONCANONICAL_UNUSED_SLOT);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsUnusedSlotWithNoncanonicalTupleLength) {
    Page page = InitializedHeapPage();
    auto unused_slot = CanonicalUnusedSlot();
    unused_slot.tuple_length = 1;
    const std::array slots{unused_slot};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::NONCANONICAL_UNUSED_SLOT);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsOutOfRangeFreeSlotHead) {
    Page page = InitializedHeapPage();
    const std::array slots{PackedNormalSlot()};
    ConfigureSlotsForValidation(page, slots, 1);

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_FREE_SLOT_HEAD);
    EXPECT_EQ(result.slot_id, SlotId{1});
}

TEST(HeapPageValidationTest, RejectsFreeSlotHeadPointingToNormalSlot) {
    Page page = InitializedHeapPage();
    const std::array slots{PackedNormalSlot()};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_FREE_SLOT_HEAD);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsFreeSlotHeadPointingToDeadSlot) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalDeadSlot()};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_FREE_SLOT_HEAD);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsOutOfRangeUnusedNextLink) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalUnusedSlot(1)};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_FREE_SLOT_LINK);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsUnusedNextLinkPointingToNormalSlot) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalUnusedSlot(1), PackedNormalSlot()};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_FREE_SLOT_LINK);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsUnusedNextLinkPointingToDeadSlot) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalUnusedSlot(1), CanonicalDeadSlot()};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_FREE_SLOT_LINK);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsSelfCycleInFreeSlotList) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalUnusedSlot(0)};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::FREE_SLOT_CYCLE);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsMultiSlotCycleInFreeSlotList) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalUnusedSlot(1), CanonicalUnusedSlot(0)};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::FREE_SLOT_CYCLE);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsUnusedSlotOmittedFromFreeSlotList) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalUnusedSlot(), CanonicalUnusedSlot()};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::FREE_SLOT_MEMBERSHIP_MISMATCH);
    EXPECT_EQ(result.slot_id, SlotId{1});
}

TEST(HeapPageValidationTest, RejectsRedirectReservedAsUnsupportedWithoutGuessingFields) {
    constexpr std::array redirect_slots{
        HeapSlotEntry{.state = HeapSlotState::REDIRECT_RESERVED},
        HeapSlotEntry{.tuple_offset = std::numeric_limits<std::uint16_t>::max(),
                      .tuple_length = std::numeric_limits<std::uint16_t>::max(),
                      .state = HeapSlotState::REDIRECT_RESERVED,
                      .aux = std::numeric_limits<std::uint16_t>::max()},
    };
    for (const auto& redirect : redirect_slots) {
        Page page = InitializedHeapPage();
        const std::array slots{PackedNormalSlot(), redirect};
        ConfigureSlotsForValidation(page, slots);

        const auto result = heap_test::TestHeapPage(page).Validate();
        EXPECT_EQ(result.error, HeapPageValidationError::UNSUPPORTED_RESERVED_STATE);
        EXPECT_EQ(result.slot_id, SlotId{1});
    }
}

TEST(HeapPageValidationTest, AcceptsRetainedAndReclaimedDeadForms) {
    Page page = InitializedHeapPage();
    heap_test::WriteTestTuple(page, 8000, 100);
    heap_test::WriteTestTuple(page, 8100, 92);
    const std::array slots{
        RetainedSlot(HeapSlotState::DEAD, 8000, 100),
        CanonicalDeadSlot(),
        RetainedSlot(HeapSlotState::NORMAL, 8100, 92),
    };
    WriteSlots(page, slots, 8000);

    EXPECT_TRUE(heap_test::TestHeapPage(page).Validate());
}

TEST(HeapPageValidationTest, RejectsNonzeroAuxOnNormalAndDeadSlots) {
    for (const auto state : {HeapSlotState::NORMAL, HeapSlotState::DEAD}) {
        Page page = InitializedHeapPage();
        heap_test::WriteTestTuple(page, 8000, 192);
        auto slot = RetainedSlot(state, 8000, 192);
        slot.aux = 1;
        const std::array slots{slot};
        WriteSlots(page, slots, 8000);

        const auto result = heap_test::TestHeapPage(page).Validate();
        EXPECT_EQ(result.error, HeapPageValidationError::NONZERO_SLOT_AUX);
        EXPECT_EQ(result.slot_id, SlotId{0});
    }

    Page page = InitializedHeapPage();
    auto reclaimed = CanonicalDeadSlot();
    reclaimed.aux = INVALID_SLOT_ID;
    const std::array slots{reclaimed};
    WriteSlots(page, slots, PAGE_SIZE);
    EXPECT_EQ(heap_test::TestHeapPage(page).Validate().error,
              HeapPageValidationError::NONZERO_SLOT_AUX);
}

TEST(HeapPageValidationTest, RejectsEmptyNormalTuple) {
    constexpr std::array empty_normals{
        HeapSlotEntry{.tuple_offset = 0, .tuple_length = 0, .state = HeapSlotState::NORMAL},
        HeapSlotEntry{.tuple_offset = static_cast<std::uint16_t>(PAGE_SIZE),
                      .tuple_length = 0,
                      .state = HeapSlotState::NORMAL},
    };
    for (const auto& slot : empty_normals) {
        Page page = InitializedHeapPage();
        const std::array slots{slot};
        WriteSlots(page, slots, PAGE_SIZE);

        const auto result = heap_test::TestHeapPage(page).Validate();
        EXPECT_EQ(result.error, HeapPageValidationError::EMPTY_NORMAL_TUPLE);
        EXPECT_EQ(result.slot_id, SlotId{0});
    }
}

TEST(HeapPageValidationTest, RejectsDeadSlotWithMixedZeroCoordinates) {
    constexpr std::array mixed_dead_slots{
        HeapSlotEntry{.tuple_offset = 8000, .tuple_length = 0, .state = HeapSlotState::DEAD},
        HeapSlotEntry{.tuple_offset = 0, .tuple_length = 100, .state = HeapSlotState::DEAD},
    };
    for (const auto& slot : mixed_dead_slots) {
        Page page = InitializedHeapPage();
        const std::array slots{slot};
        WriteSlots(page, slots, 8000);

        const auto result = heap_test::TestHeapPage(page).Validate();
        EXPECT_EQ(result.error, HeapPageValidationError::NONCANONICAL_DEAD_SLOT);
        EXPECT_EQ(result.slot_id, SlotId{0});
    }
}

TEST(HeapPageValidationTest, RejectsRetainedDeadRangesOutsideTupleRegionOrPage) {
    struct Range {
        std::uint16_t offset;
        std::uint16_t length;
    };
    constexpr std::array invalid_ranges{
        Range{.offset = 7999, .length = 100},
        Range{.offset = 8180, .length = 20},
        Range{.offset = std::numeric_limits<std::uint16_t>::max(),
              .length = std::numeric_limits<std::uint16_t>::max()},
    };
    for (const auto range : invalid_ranges) {
        Page page = InitializedHeapPage();
        const std::array slots{RetainedSlot(HeapSlotState::DEAD, range.offset, range.length)};
        WriteSlots(page, slots, 8000);

        const auto result = heap_test::TestHeapPage(page).Validate();
        EXPECT_EQ(result.error, HeapPageValidationError::RETAINED_TUPLE_OUT_OF_BOUNDS);
        EXPECT_EQ(result.slot_id, SlotId{0});
    }
}

TEST(HeapPageValidationTest, RejectsOverlappingRetainedRangesInAnyStateCombination) {
    struct Case {
        HeapSlotState first_state;
        std::uint16_t first_offset;
        std::uint16_t first_length;
        HeapSlotState second_state;
        std::uint16_t second_offset;
        std::uint16_t second_length;
    };
    constexpr std::array cases{
        // Partial overlap between two NORMAL tuples.
        Case{HeapSlotState::NORMAL, 8000, 100, HeapSlotState::NORMAL, 8099, 93},
        // A NORMAL tuple wholly inside a retained DEAD range.
        Case{HeapSlotState::DEAD, 8000, 192, HeapSlotState::NORMAL, 8050, 60},
        // Two slots claiming the identical range.
        Case{HeapSlotState::DEAD, 8000, 100, HeapSlotState::DEAD, 8000, 100},
    };
    for (const auto& test_case : cases) {
        Page page = InitializedHeapPage();
        const std::array slots{
            RetainedSlot(test_case.first_state, test_case.first_offset, test_case.first_length),
            RetainedSlot(test_case.second_state, test_case.second_offset, test_case.second_length),
        };
        WriteSlots(page, slots, 8000);

        const auto result = heap_test::TestHeapPage(page).Validate();
        EXPECT_EQ(result.error, HeapPageValidationError::OVERLAPPING_TUPLE_RANGES);
        EXPECT_EQ(result.slot_id, SlotId{1});
        EXPECT_EQ(result.other_slot_id, SlotId{0});
    }
}

TEST(HeapPageValidationTest, AcceptsAdjacentRangesAndUnownedHoles) {
    Page page = InitializedHeapPage();
    heap_test::WriteTestTuple(page, 7900, 100);
    heap_test::WriteTestTuple(page, 8000, 100);
    // Bytes 8100..8129 are an unowned hole; their contents are ignored.
    std::ranges::fill(page.Bytes().subspan(8100, 30), std::byte{0xEE});
    heap_test::WriteTestTuple(page, 8130, 62);
    const std::array slots{
        RetainedSlot(HeapSlotState::NORMAL, 8130, 62),
        RetainedSlot(HeapSlotState::DEAD, 8000, 100),
        RetainedSlot(HeapSlotState::NORMAL, 7900, 100),
    };
    WriteSlots(page, slots, 7900);

    EXPECT_TRUE(heap_test::TestHeapPage(page).Validate());
}

TEST(HeapPageValidationTest, RejectsMalformedNormalAndRetainedDeadTupleBytes) {
    for (const auto state : {HeapSlotState::NORMAL, HeapSlotState::DEAD}) {
        Page page = InitializedHeapPage();
        // Zero bytes do not form a tuple: header_bytes must be 48.
        const std::array slots{RetainedSlot(state, 8000, 192)};
        WriteSlots(page, slots, 8000);

        const auto result = heap_test::TestHeapPage(page).Validate();
        EXPECT_EQ(result.error, HeapPageValidationError::INVALID_TUPLE);
        EXPECT_EQ(result.slot_id, SlotId{0});
        EXPECT_EQ(result.tuple_header_error, TupleHeaderDecodeError::INVALID_HEADER_SIZE);
    }
}

TEST(HeapPageValidationTest, RejectsRetainedRangeShorterThanTupleHeader) {
    Page page = InitializedHeapPage();
    heap_test::WriteTestTuple(page, 8000, 192);
    const std::array slots{RetainedSlot(HeapSlotState::NORMAL, 8000, 47)};
    WriteSlots(page, slots, 8000);

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_TUPLE);
    EXPECT_EQ(result.tuple_header_error, TupleHeaderDecodeError::SOURCE_TOO_SMALL);
}

TEST(HeapPageValidationTest, RejectsSlotLengthThatDisagreesWithTupleEncoding) {
    // A slot one byte longer than the encoded tuple leaves a trailing unreferenced byte; one byte
    // shorter truncates the VARCHAR payload. Neither is a complete tuple.
    for (const std::uint16_t slot_length : {std::uint16_t{101}, std::uint16_t{99}}) {
        Page page = InitializedHeapPage();
        heap_test::WriteTestTuple(page, 8000, 100);
        const std::array slots{RetainedSlot(HeapSlotState::NORMAL, 8000, slot_length)};
        WriteSlots(page, slots, 8000);

        const auto result = heap_test::TestHeapPage(page).Validate();
        EXPECT_EQ(result.error, HeapPageValidationError::INVALID_TUPLE);
        EXPECT_NE(result.tuple_error, TupleCodecError::NONE);
    }
}

TEST(HeapPageValidationTest, RejectsTupleWithSchemaInvalidContent) {
    Page page = InitializedHeapPage();
    heap_test::WriteTestTuple(page, 8000, 100);
    // Set the NOT NULL column's null bit; tuple bytes are otherwise intact.
    page.Bytes()[8000 + TUPLE_HEADER_ENCODED_SIZE] = std::byte{0x01};
    const std::array slots{RetainedSlot(HeapSlotState::DEAD, 8000, 100)};
    WriteSlots(page, slots, 8000);

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_TUPLE);
    EXPECT_NE(result.tuple_error, TupleCodecError::NONE);
}

TEST(HeapPageValidationTest, RejectsTupleWhoseSchemaVersionIsNotRegistered) {
    for (const SchemaVer schema_version : {SchemaVer{0}, SchemaVer{2}}) {
        Page page = InitializedHeapPage();
        heap_test::WriteTestTuple(page, 8000, 100);
        ASSERT_TRUE(EncodeLittleEndian(
            page.Bytes().subspan(8000 + TUPLE_HEADER_SCHEMA_VERSION_OFFSET, sizeof(SchemaVer)),
            schema_version));
        const std::array slots{RetainedSlot(HeapSlotState::NORMAL, 8000, 100)};
        WriteSlots(page, slots, 8000);

        const auto result = heap_test::TestHeapPage(page).Validate();
        EXPECT_EQ(result.error, HeapPageValidationError::UNRESOLVED_SCHEMA_VERSION);
        EXPECT_EQ(result.slot_id, SlotId{0});
    }
}

TEST(HeapPageValidationTest, RejectsEveryTupleWhenNoSchemaIsRegistered) {
    Page page = InitializedHeapPage();
    ConfigureOneNormalSlot(page);
    const HeapPage heap_page{page, HeapTupleSchemaHistory{{}}};

    EXPECT_EQ(heap_page.Validate().error, HeapPageValidationError::UNRESOLVED_SCHEMA_VERSION);
}

TEST(HeapPageValidationTest, AppliesTupleTransactionFieldDomains) {
    struct Case {
        TupleVersionMetadata metadata;
        bool valid;
    };
    const std::array cases{
        Case{.metadata = {.xmin = INVALID_TXN_ID}, .valid = false},
        Case{.metadata = {.xmin = FIRST_NORMAL_TXN_ID, .xmax = FROZEN_TXN_ID, .cmax = 1},
             .valid = false},
        Case{.metadata = {.xmin = FIRST_NORMAL_TXN_ID, .cmax = 1}, .valid = false},
        Case{.metadata = {.xmin = FROZEN_TXN_ID}, .valid = true},
        Case{.metadata = {.xmin = FIRST_NORMAL_TXN_ID,
                          .xmax = FIRST_NORMAL_TXN_ID + 1,
                          .cmin = 3,
                          .cmax = 4},
             .valid = true},
    };
    for (const auto& test_case : cases) {
        Page page = InitializedHeapPage();
        heap_test::WriteTestTuple(page, 8000, 100, std::byte{0x5A}, test_case.metadata);
        const std::array slots{RetainedSlot(HeapSlotState::NORMAL, 8000, 100)};
        WriteSlots(page, slots, 8000);

        const auto result = heap_test::TestHeapPage(page).Validate();
        if (test_case.valid) {
            EXPECT_TRUE(result);
        } else {
            EXPECT_EQ(result.error, HeapPageValidationError::INVALID_TUPLE_TRANSACTION_FIELDS);
        }
    }
}

TEST(HeapPageValidationTest, AppliesBoundedPreviousVersionPointerChecks) {
    // The page under test is page 3; slot 0 holds the checked tuple and slot 1 a neighbor.
    struct Case {
        PageNo prev_page_no;
        SlotId prev_slot;
        bool valid;
    };
    constexpr std::array cases{
        Case{.prev_page_no = 0, .prev_slot = 0, .valid = false},
        Case{.prev_page_no = 3, .prev_slot = 0, .valid = false},
        Case{.prev_page_no = 3, .prev_slot = 2, .valid = false},
        Case{.prev_page_no = 3, .prev_slot = 1, .valid = true},
        Case{.prev_page_no = 1, .prev_slot = 0, .valid = true},
        Case{.prev_page_no = 900, .prev_slot = 4000, .valid = true},
    };
    for (const auto& test_case : cases) {
        Page page = InitializedHeapPage({.file_id = 7, .page_no = 3});
        TupleVersionMetadata metadata = heap_test::TestTupleMetadata();
        metadata.prev_page_no = test_case.prev_page_no;
        metadata.prev_slot = test_case.prev_slot;
        heap_test::WriteTestTuple(page, 8000, 100, std::byte{0x5A}, metadata);
        heap_test::WriteTestTuple(page, 8100, 92);
        const std::array slots{
            RetainedSlot(HeapSlotState::NORMAL, 8000, 100),
            RetainedSlot(HeapSlotState::DEAD, 8100, 92),
        };
        WriteSlots(page, slots, 8000);

        const auto result = heap_test::TestHeapPage(page).Validate();
        if (test_case.valid) {
            EXPECT_TRUE(result) << test_case.prev_page_no << "/" << test_case.prev_slot;
        } else {
            EXPECT_EQ(result.error, HeapPageValidationError::INVALID_PREVIOUS_VERSION_POINTER)
                << test_case.prev_page_no << "/" << test_case.prev_slot;
            EXPECT_EQ(result.slot_id, SlotId{0});
        }
    }
}

TEST(HeapPageValidationTest, ResolvesEachTupleAgainstItsOwnHistoricalSchema) {
    constexpr std::array v1_columns{
        PhysicalColumnSpec{.type = PhysicalType::INT32, .nullable = false},
    };
    constexpr std::array v2_columns{
        PhysicalColumnSpec{.type = PhysicalType::INT64, .nullable = true},
        PhysicalColumnSpec{.type = PhysicalType::BOOLEAN, .nullable = false},
    };
    auto v1 = BuildTuplePhysicalLayout(v1_columns, SchemaVer{1});
    auto v2 = BuildTuplePhysicalLayout(v2_columns, SchemaVer{2});
    if (!v1.layout.has_value() || !v2.layout.has_value()) {
        ADD_FAILURE() << "historical layouts did not build";
        return;
    }
    const std::array layouts{*v1.layout, *v2.layout};

    const std::array v1_values{TupleValue{std::int32_t{-4}}};
    const std::array v2_values{TupleValue{std::monostate{}}, TupleValue{true}};
    const auto v1_tuple = EncodeTuple(layouts[0], heap_test::TestTupleMetadata(), v1_values);
    const auto v2_tuple = EncodeTuple(layouts[1], heap_test::TestTupleMetadata(), v2_values);
    if (!v1_tuple.tuple.has_value() || !v2_tuple.tuple.has_value()) {
        ADD_FAILURE() << "historical tuples did not encode";
        return;
    }

    Page page = InitializedHeapPage();
    HeapPage both{page, HeapTupleSchemaHistory{layouts}};
    ASSERT_TRUE(both.Insert(*v1_tuple.tuple));
    ASSERT_TRUE(both.Insert(*v2_tuple.tuple));
    EXPECT_TRUE(both.Validate());

    const HeapPage only_v1{page, HeapTupleSchemaHistory{std::span{layouts}.first(1)}};
    const auto result = only_v1.Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::UNRESOLVED_SCHEMA_VERSION);
    EXPECT_EQ(result.slot_id, SlotId{1});
}

TEST(HeapPageValidationTest, ReportsLowestSlotForDeterministicTupleFailure) {
    Page page = InitializedHeapPage();
    heap_test::WriteTestTuple(page, 8000, 100, std::byte{0x5A}, TupleVersionMetadata{});
    heap_test::WriteTestTuple(page, 8100, 92, std::byte{0x5A}, TupleVersionMetadata{});
    const std::array slots{
        RetainedSlot(HeapSlotState::NORMAL, 8100, 92),
        RetainedSlot(HeapSlotState::NORMAL, 8000, 100),
    };
    WriteSlots(page, slots, 8000);

    const auto result = heap_test::TestHeapPage(page).Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_TUPLE_TRANSACTION_FIELDS);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

} // namespace
} // namespace dblusblus
