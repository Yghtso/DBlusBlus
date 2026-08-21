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

[[nodiscard]] constexpr HeapSlotEntry ZeroLengthNormalSlot() noexcept {
    return HeapSlotEntry{
        .tuple_offset = static_cast<std::uint16_t>(PAGE_SIZE),
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

TEST(HeapPageTest, InitializesDeterministicBlankHeapDataPage) {
    Page page{PageId{.file_id = 17, .page_no = 23}};
    std::ranges::fill(page.Bytes(), std::byte{0xA5});
    HeapPage heap_page{page};

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
    HeapPage heap_page{page};

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

        EXPECT_FALSE(HeapPage{page}.Initialize());
        EXPECT_TRUE(std::ranges::equal(page.Bytes(), original));
    }
}

TEST(HeapPageTest, InitializationWritesZeroFlagsAndPreservesExplicitPageLsn) {
    Page page{PageId{.file_id = 18, .page_no = 24}};
    HeapPage heap_page{page};
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

    const auto result = HeapPage{page}.Validate();
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
    EXPECT_EQ(HeapPage{page}.Validate().error, HeapPageValidationError::WRONG_PAGE_TYPE);

    common_header = original_header;
    common_header.page_no += 1;
    WriteCommonHeader(page, common_header);
    EXPECT_EQ(HeapPage{page}.Validate().error, HeapPageValidationError::WRONG_PAGE_NUMBER);

    common_header = original_header;
    common_header.header_size = COMMON_PAGE_HEADER_ENCODED_SIZE;
    WriteCommonHeader(page, common_header);
    EXPECT_EQ(HeapPage{page}.Validate().error, HeapPageValidationError::WRONG_HEADER_SIZE);

    common_header = original_header;
    common_header.format_version = HEAP_PAGE_FORMAT_VERSION + 1;
    WriteCommonHeader(page, common_header);
    EXPECT_EQ(HeapPage{page}.Validate().error, HeapPageValidationError::UNSUPPORTED_FORMAT_VERSION);

    common_header = original_header;
    common_header.reserved16 = 1;
    WriteCommonHeader(page, common_header);
    EXPECT_EQ(HeapPage{page}.Validate().error, HeapPageValidationError::NONZERO_COMMON_RESERVED);
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

        EXPECT_EQ(HeapPage{page}.Validate().error, HeapPageValidationError::INVALID_PAGE_NUMBER);
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

        EXPECT_EQ(HeapPage{page}.Validate().error, HeapPageValidationError::NONZERO_COMMON_FLAGS);
    }
}

TEST(HeapPageValidationTest, RejectsInvalidHeapHeaderGeometryAndReservedField) {
    Page page = InitializedHeapPage();
    auto header = HeapPageHeader{};

    header.lower = HEAP_PAGE_TOTAL_HEADER_SIZE - 1;
    WriteHeapHeader(page, header);
    EXPECT_EQ(HeapPage{page}.Validate().error, HeapPageValidationError::LOWER_BEFORE_HEADER);

    header = HeapPageHeader{};
    header.upper = PAGE_SIZE + 1;
    WriteHeapHeader(page, header);
    EXPECT_EQ(HeapPage{page}.Validate().error, HeapPageValidationError::UPPER_AFTER_PAGE);

    header = HeapPageHeader{};
    header.lower = 100;
    header.upper = 99;
    WriteHeapHeader(page, header);
    EXPECT_EQ(HeapPage{page}.Validate().error, HeapPageValidationError::LOWER_AFTER_UPPER);

    header = HeapPageHeader{};
    header.slot_count = std::numeric_limits<std::uint16_t>::max();
    WriteHeapHeader(page, header);
    EXPECT_EQ(HeapPage{page}.Validate().error,
              HeapPageValidationError::SLOT_DIRECTORY_OUT_OF_BOUNDS);

    header = HeapPageHeader{};
    header.slot_count = 1;
    WriteHeapHeader(page, header);
    EXPECT_EQ(HeapPage{page}.Validate().error, HeapPageValidationError::SLOT_COUNT_LOWER_MISMATCH);

    header = HeapPageHeader{};
    header.free_slot_head = 0;
    WriteHeapHeader(page, header);
    EXPECT_EQ(HeapPage{page}.Validate().error, HeapPageValidationError::INVALID_FREE_SLOT_HEAD);

    header = HeapPageHeader{};
    header.reserved = 1;
    WriteHeapHeader(page, header);
    EXPECT_EQ(HeapPage{page}.Validate().error, HeapPageValidationError::NONZERO_HEAP_RESERVED);
}

TEST(HeapPageValidationTest, RejectsInvalidSlotState) {
    Page page = InitializedHeapPage();
    ConfigureOneNormalSlot(page);
    ASSERT_TRUE(EncodeLittleEndian(SlotBytes(page, 0).subspan(HEAP_SLOT_FLAGS_OFFSET, 2),
                                   std::uint16_t{99}));

    const auto result = HeapPage{page}.Validate();
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_SLOT_STATE);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsNormalTupleRangesOutsideTupleRegionOrPage) {
    Page page = InitializedHeapPage();

    ConfigureOneNormalSlot(page, 7999, 1);
    auto result = HeapPage{page}.Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::NORMAL_TUPLE_OUT_OF_BOUNDS);
    EXPECT_EQ(result.slot_id, SlotId{0});

    ConfigureOneNormalSlot(page, 8180, 20);
    result = HeapPage{page}.Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::NORMAL_TUPLE_OUT_OF_BOUNDS);

    ConfigureOneNormalSlot(page, std::numeric_limits<std::uint16_t>::max() - 5U, 10);
    result = HeapPage{page}.Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::NORMAL_TUPLE_OUT_OF_BOUNDS);
}

TEST(HeapPageValidationTest, AcceptsCanonicalSingleElementFreeListWithSentinelTermination) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalUnusedSlot()};
    ConfigureSlotsForValidation(page, slots, 0);

    EXPECT_TRUE(HeapPage{page}.Validate());
}

TEST(HeapPageValidationTest, AcceptsCanonicalMultiElementFreeList) {
    Page page = InitializedHeapPage();
    const std::array slots{
        CanonicalUnusedSlot(2),
        CanonicalUnusedSlot(),
        CanonicalUnusedSlot(1),
    };
    ConfigureSlotsForValidation(page, slots, 0);

    EXPECT_TRUE(HeapPage{page}.Validate());
}

TEST(HeapPageValidationTest, RejectsUnusedSlotWithNoncanonicalTupleOffset) {
    Page page = InitializedHeapPage();
    auto unused_slot = CanonicalUnusedSlot();
    unused_slot.tuple_offset = 1;
    const std::array slots{unused_slot};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = HeapPage{page}.Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::NONCANONICAL_UNUSED_SLOT);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsUnusedSlotWithNoncanonicalTupleLength) {
    Page page = InitializedHeapPage();
    auto unused_slot = CanonicalUnusedSlot();
    unused_slot.tuple_length = 1;
    const std::array slots{unused_slot};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = HeapPage{page}.Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::NONCANONICAL_UNUSED_SLOT);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsOutOfRangeFreeSlotHead) {
    Page page = InitializedHeapPage();
    const std::array slots{ZeroLengthNormalSlot()};
    ConfigureSlotsForValidation(page, slots, 1);

    const auto result = HeapPage{page}.Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_FREE_SLOT_HEAD);
    EXPECT_EQ(result.slot_id, SlotId{1});
}

TEST(HeapPageValidationTest, RejectsFreeSlotHeadPointingToNormalSlot) {
    Page page = InitializedHeapPage();
    const std::array slots{ZeroLengthNormalSlot()};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = HeapPage{page}.Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_FREE_SLOT_HEAD);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsFreeSlotHeadPointingToDeadSlot) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalDeadSlot()};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = HeapPage{page}.Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_FREE_SLOT_HEAD);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsOutOfRangeUnusedNextLink) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalUnusedSlot(1)};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = HeapPage{page}.Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_FREE_SLOT_LINK);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsUnusedNextLinkPointingToNormalSlot) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalUnusedSlot(1), ZeroLengthNormalSlot()};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = HeapPage{page}.Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_FREE_SLOT_LINK);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsUnusedNextLinkPointingToDeadSlot) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalUnusedSlot(1), CanonicalDeadSlot()};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = HeapPage{page}.Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::INVALID_FREE_SLOT_LINK);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsSelfCycleInFreeSlotList) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalUnusedSlot(0)};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = HeapPage{page}.Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::FREE_SLOT_CYCLE);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsMultiSlotCycleInFreeSlotList) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalUnusedSlot(1), CanonicalUnusedSlot(0)};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = HeapPage{page}.Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::FREE_SLOT_CYCLE);
    EXPECT_EQ(result.slot_id, SlotId{0});
}

TEST(HeapPageValidationTest, RejectsUnusedSlotOmittedFromFreeSlotList) {
    Page page = InitializedHeapPage();
    const std::array slots{CanonicalUnusedSlot(), CanonicalUnusedSlot()};
    ConfigureSlotsForValidation(page, slots, 0);

    const auto result = HeapPage{page}.Validate();
    EXPECT_EQ(result.error, HeapPageValidationError::FREE_SLOT_MEMBERSHIP_MISMATCH);
    EXPECT_EQ(result.slot_id, SlotId{1});
}

TEST(HeapPageValidationTest, DoesNotInventTupleRangeSemanticsForDeadAndRedirectReservedStates) {
    Page page = InitializedHeapPage();
    WriteHeapHeader(
        page,
        HeapPageHeader{
            .slot_count = 2,
            .free_slot_head = INVALID_SLOT_ID,
            .lower = HEAP_PAGE_TOTAL_HEADER_SIZE + (2 * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE),
            .upper = PAGE_SIZE,
        });
    ASSERT_TRUE(
        EncodeHeapSlotEntry(SlotBytes(page, 0),
                            HeapSlotEntry{.tuple_offset = std::numeric_limits<std::uint16_t>::max(),
                                          .tuple_length = std::numeric_limits<std::uint16_t>::max(),
                                          .state = HeapSlotState::DEAD,
                                          .aux = std::numeric_limits<std::uint16_t>::max()}));
    ASSERT_TRUE(
        EncodeHeapSlotEntry(SlotBytes(page, 1),
                            HeapSlotEntry{.tuple_offset = std::numeric_limits<std::uint16_t>::max(),
                                          .tuple_length = std::numeric_limits<std::uint16_t>::max(),
                                          .state = HeapSlotState::REDIRECT_RESERVED,
                                          .aux = std::numeric_limits<std::uint16_t>::max()}));

    EXPECT_TRUE(HeapPage{page}.Validate());
}

} // namespace
} // namespace dblusblus
