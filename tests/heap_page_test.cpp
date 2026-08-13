#include "common/encoding.h"
#include "common/file_superblock.h"
#include "common/page_header.h"
#include "storage/disk_manager.h"
#include "storage/heap_page.h"
#include "storage/page.h"
#include "storage/page_file.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unistd.h>

namespace dblusblus {
namespace {

static_assert(HEAP_PAGE_HEADER_OFFSET == 32);
static_assert(HEAP_PAGE_HEADER_ENCODED_SIZE == 16);
static_assert(HEAP_PAGE_TOTAL_HEADER_SIZE == 48);
static_assert(HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE == 8);
static_assert(HEAP_PAGE_SLOT_DIRECTORY_OFFSET == 48);
static_assert(HEAP_PAGE_MAX_RAW_TUPLE_SIZE == 8135);
static_assert(std::is_same_v<std::underlying_type_t<HeapSlotState>, std::uint16_t>);

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        std::array path_template{
            '/', 't', 'm', 'p', '/', 'd', 'b', 'l', 'u', 's', 'b', 'l', 'u',  's',
            '-', 'h', 'e', 'a', 'p', '-', 'X', 'X', 'X', 'X', 'X', 'X', '\0',
        };
        if (char* created = ::mkdtemp(path_template.data()); created != nullptr) {
            path_ = created;
        }
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] bool valid() const noexcept {
        return !path_.empty();
    }

    [[nodiscard]] std::filesystem::path File(std::string_view name) const {
        return path_ / name;
    }

  private:
    std::filesystem::path path_;
};

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

TEST(HeapPageHeaderCodecTest, EmitsExactSixteenByteLittleEndianLayout) {
    const HeapPageHeader header{
        .slot_count = 0x0201,
        .free_slot_head = 0x0403,
        .lower = 0x0605,
        .upper = 0x0807,
        .prune_hint = 0x0C0B0A09U,
        .reserved = 0x100F0E0DU,
    };
    std::array<std::byte, HEAP_PAGE_HEADER_ENCODED_SIZE> encoded{};
    constexpr std::array expected{
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04},
        std::byte{0x05},
        std::byte{0x06},
        std::byte{0x07},
        std::byte{0x08},
        std::byte{0x09},
        std::byte{0x0A},
        std::byte{0x0B},
        std::byte{0x0C},
        std::byte{0x0D},
        std::byte{0x0E},
        std::byte{0x0F},
        std::byte{0x10},
    };

    ASSERT_TRUE(EncodeHeapPageHeader(encoded, header));
    EXPECT_EQ(encoded, expected);
    const auto decoded = DecodeHeapPageHeader(encoded);
    if (!decoded.has_value()) {
        ADD_FAILURE() << "heap header decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(*decoded, header);
}

TEST(HeapPageHeaderCodecTest, RoundTripsIntegerBoundaries) {
    constexpr HeapPageHeader minimum{
        .slot_count = 0,
        .free_slot_head = 0,
        .lower = 0,
        .upper = 0,
        .prune_hint = 0,
        .reserved = 0,
    };
    constexpr HeapPageHeader maximum{
        .slot_count = std::numeric_limits<std::uint16_t>::max(),
        .free_slot_head = std::numeric_limits<SlotId>::max(),
        .lower = std::numeric_limits<std::uint16_t>::max(),
        .upper = std::numeric_limits<std::uint16_t>::max(),
        .prune_hint = std::numeric_limits<std::uint32_t>::max(),
        .reserved = std::numeric_limits<std::uint32_t>::max(),
    };

    for (const auto& expected : {minimum, maximum}) {
        std::array<std::byte, HEAP_PAGE_HEADER_ENCODED_SIZE> encoded{};
        ASSERT_TRUE(EncodeHeapPageHeader(encoded, expected));
        const auto decoded = DecodeHeapPageHeader(encoded);
        if (!decoded.has_value()) {
            ADD_FAILURE() << "heap header boundary decode unexpectedly failed";
            continue;
        }
        EXPECT_EQ(*decoded, expected);
    }
}

TEST(HeapPageHeaderCodecTest, SupportsUnalignedSpans) {
    constexpr auto padding = std::byte{0xA5};
    const HeapPageHeader expected{
        .slot_count = 3,
        .free_slot_head = 5,
        .lower = 72,
        .upper = 7000,
        .prune_hint = 11,
        .reserved = 13,
    };
    std::array<std::byte, HEAP_PAGE_HEADER_ENCODED_SIZE + 2> buffer{};
    buffer.fill(padding);
    auto unaligned = std::span<std::byte>{buffer}.subspan(1, HEAP_PAGE_HEADER_ENCODED_SIZE);

    ASSERT_TRUE(EncodeHeapPageHeader(unaligned, expected));
    EXPECT_EQ(buffer.front(), padding);
    EXPECT_EQ(buffer.back(), padding);
    const auto decoded = DecodeHeapPageHeader(unaligned);
    if (!decoded.has_value()) {
        ADD_FAILURE() << "unaligned heap header decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(*decoded, expected);
}

TEST(HeapPageHeaderCodecTest, RejectsUndersizedBuffersWithoutModification) {
    constexpr auto padding = std::byte{0xA5};
    std::array<std::byte, HEAP_PAGE_HEADER_ENCODED_SIZE> buffer{};
    buffer.fill(padding);
    const auto original = buffer;
    auto undersized = std::span<std::byte>{buffer}.first(HEAP_PAGE_HEADER_ENCODED_SIZE - 1);

    EXPECT_FALSE(EncodeHeapPageHeader(undersized, HeapPageHeader{}));
    EXPECT_EQ(buffer, original);
    EXPECT_FALSE(DecodeHeapPageHeader(undersized).has_value());
}

TEST(HeapSlotEntryCodecTest, EmitsExactEightByteLayoutAndRoundTrips) {
    const HeapSlotEntry expected{
        .tuple_offset = 0x0201,
        .tuple_length = 0x0403,
        .state = HeapSlotState::DEAD,
        .aux = 0x0807,
    };
    std::array<std::byte, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE> encoded{};
    constexpr std::array expected_bytes{
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04},
        std::byte{0x02},
        std::byte{0x00},
        std::byte{0x07},
        std::byte{0x08},
    };

    ASSERT_TRUE(EncodeHeapSlotEntry(encoded, expected));
    EXPECT_EQ(encoded, expected_bytes);
    const auto decoded = DecodeHeapSlotEntry(encoded);
    if (!decoded.entry.has_value()) {
        ADD_FAILURE() << "slot entry decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(decoded.error, HeapSlotEntryDecodeError::NONE);
    EXPECT_EQ(*decoded.entry, expected);
}

TEST(HeapSlotEntryCodecTest, PinsEveryExplicitPersistedSlotStateCode) {
    struct StateCode {
        HeapSlotState state;
        std::uint16_t code;
    };
    constexpr std::array states{
        StateCode{.state = HeapSlotState::UNUSED, .code = 0},
        StateCode{.state = HeapSlotState::NORMAL, .code = 1},
        StateCode{.state = HeapSlotState::DEAD, .code = 2},
        StateCode{.state = HeapSlotState::REDIRECT_RESERVED, .code = 3},
    };

    for (const auto& state : states) {
        std::array<std::byte, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE> encoded{};
        ASSERT_TRUE(EncodeHeapSlotEntry(encoded, HeapSlotEntry{.state = state.state}));
        EXPECT_EQ(encoded[HEAP_SLOT_FLAGS_OFFSET], static_cast<std::byte>(state.code));
        EXPECT_EQ(encoded[HEAP_SLOT_FLAGS_OFFSET + 1], std::byte{0});

        const auto decoded = DecodeHeapSlotEntry(encoded);
        if (!decoded.entry.has_value()) {
            ADD_FAILURE() << "slot state decode unexpectedly failed";
            continue;
        }
        EXPECT_EQ(decoded.entry->state, state.state);
    }
}

TEST(HeapSlotEntryCodecTest, SupportsUnalignedSpansAndIntegerBoundaries) {
    constexpr auto padding = std::byte{0xA5};
    const HeapSlotEntry expected{
        .tuple_offset = std::numeric_limits<std::uint16_t>::max(),
        .tuple_length = std::numeric_limits<std::uint16_t>::max(),
        .state = HeapSlotState::REDIRECT_RESERVED,
        .aux = std::numeric_limits<std::uint16_t>::max(),
    };
    std::array<std::byte, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE + 2> buffer{};
    buffer.fill(padding);
    auto unaligned = std::span<std::byte>{buffer}.subspan(1, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE);

    ASSERT_TRUE(EncodeHeapSlotEntry(unaligned, expected));
    EXPECT_EQ(buffer.front(), padding);
    EXPECT_EQ(buffer.back(), padding);
    const auto decoded = DecodeHeapSlotEntry(unaligned);
    if (!decoded.entry.has_value()) {
        ADD_FAILURE() << "unaligned slot entry decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(*decoded.entry, expected);
}

TEST(HeapSlotEntryCodecTest, RejectsUndersizedAndInvalidStateWithoutModification) {
    constexpr auto padding = std::byte{0xA5};
    std::array<std::byte, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE> buffer{};
    buffer.fill(padding);
    const auto original = buffer;
    auto undersized = std::span<std::byte>{buffer}.first(HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE - 1);

    EXPECT_FALSE(EncodeHeapSlotEntry(undersized, HeapSlotEntry{}));
    EXPECT_EQ(buffer, original);
    EXPECT_EQ(DecodeHeapSlotEntry(undersized).error, HeapSlotEntryDecodeError::BUFFER_TOO_SMALL);

    const HeapSlotEntry invalid{.state = static_cast<HeapSlotState>(99)};
    EXPECT_FALSE(EncodeHeapSlotEntry(buffer, invalid));
    EXPECT_EQ(buffer, original);

    std::array<std::byte, HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE> invalid_bytes{};
    ASSERT_TRUE(EncodeLittleEndian(std::span{invalid_bytes}.subspan(HEAP_SLOT_FLAGS_OFFSET, 2),
                                   std::uint16_t{99}));
    const auto decoded = DecodeHeapSlotEntry(invalid_bytes);
    EXPECT_FALSE(decoded.entry.has_value());
    EXPECT_EQ(decoded.error, HeapSlotEntryDecodeError::INVALID_SLOT_STATE);
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

TEST(HeapPageTest, InitializationPreservesExplicitFlagsAndPageLsn) {
    Page page{PageId{.file_id = 18, .page_no = 24}};
    HeapPage heap_page{page};
    ASSERT_TRUE(heap_page.Initialize(0xA5F00F5AU, Lsn{77}));

    const auto common_header = page.DecodeHeader();
    if (!common_header.has_value()) {
        ADD_FAILURE() << "initialized common header decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(common_header->flags, std::uint32_t{0xA5F00F5AU});
    EXPECT_EQ(common_header->page_lsn, Lsn{77});
    EXPECT_EQ(common_header->checksum_crc32c, std::uint32_t{0});
}

TEST(HeapPageValidationTest, AcceptsStructurallyValidNormalSlotRange) {
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
    EXPECT_EQ(HeapPage{page}.Validate().error,
              HeapPageValidationError::INVALID_EMPTY_FREE_SLOT_HEAD);

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

TEST(HeapPageValidationTest, DoesNotInventTupleRangeSemanticsForReservedStates) {
    Page page = InitializedHeapPage();
    WriteHeapHeader(
        page,
        HeapPageHeader{
            .slot_count = 3,
            .free_slot_head = INVALID_SLOT_ID,
            .lower = HEAP_PAGE_TOTAL_HEADER_SIZE + (3 * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE),
            .upper = PAGE_SIZE,
        });
    ASSERT_TRUE(
        EncodeHeapSlotEntry(SlotBytes(page, 0),
                            HeapSlotEntry{.tuple_offset = std::numeric_limits<std::uint16_t>::max(),
                                          .tuple_length = std::numeric_limits<std::uint16_t>::max(),
                                          .state = HeapSlotState::UNUSED,
                                          .aux = std::numeric_limits<std::uint16_t>::max()}));
    ASSERT_TRUE(
        EncodeHeapSlotEntry(SlotBytes(page, 1),
                            HeapSlotEntry{.tuple_offset = std::numeric_limits<std::uint16_t>::max(),
                                          .tuple_length = std::numeric_limits<std::uint16_t>::max(),
                                          .state = HeapSlotState::DEAD,
                                          .aux = std::numeric_limits<std::uint16_t>::max()}));
    ASSERT_TRUE(
        EncodeHeapSlotEntry(SlotBytes(page, 2),
                            HeapSlotEntry{.tuple_offset = std::numeric_limits<std::uint16_t>::max(),
                                          .tuple_length = std::numeric_limits<std::uint16_t>::max(),
                                          .state = HeapSlotState::REDIRECT_RESERVED,
                                          .aux = std::numeric_limits<std::uint16_t>::max()}));

    EXPECT_TRUE(HeapPage{page}.Validate());
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
        ConfigureOneNormalSlot(disallowed);
        auto slot = DecodeHeapSlotEntry(SlotBytes(disallowed, 0));
        if (!slot.entry.has_value()) {
            ADD_FAILURE() << "configured slot did not decode";
            continue;
        }
        slot.entry->state = state;
        ASSERT_TRUE(EncodeHeapSlotEntry(SlotBytes(disallowed, 0), *slot.entry));
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

TEST(HeapPageDeadTransitionIntegrationTest, PersistsDeadStateWithoutRemovingTupleBytes) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("heap-dead.pages");
    constexpr FileId file_id = 42;
    constexpr std::uint64_t object_id = 7002;
    PageId page_id{};

    {
        DiskManager manager;
        auto page_file = PageFile::Create(manager,
                                          path,
                                          FileSuperblock{
                                              .file_kind = FileKind::HEAP,
                                              .file_id = file_id,
                                              .object_id = object_id,
                                              .creation_epoch = 9002,
                                          });
        if (!page_file.page_file.has_value()) {
            ADD_FAILURE() << "heap page file creation failed";
            return;
        }
        const auto allocation = page_file.page_file->AllocatePage();
        if (!allocation.page_id.has_value()) {
            ADD_FAILURE() << "heap page allocation failed";
            return;
        }
        page_id = *allocation.page_id;

        Page page{page_id};
        HeapPage heap_page{page};
        ASSERT_TRUE(heap_page.Initialize());
        constexpr std::array first{std::byte{0x01}, std::byte{0x02}};
        constexpr std::array second{std::byte{0x10}, std::byte{0x00}, std::byte{0xFF}};
        constexpr std::array third{std::byte{0x20}};
        ASSERT_TRUE(heap_page.Insert(first));
        ASSERT_TRUE(heap_page.Insert(second));
        ASSERT_TRUE(heap_page.Insert(third));
        ASSERT_TRUE(heap_page.MarkDead(1));
        ASSERT_TRUE(manager.WritePage(page_id, page.Bytes()));
        ASSERT_TRUE(manager.SyncFile(file_id));
    }

    DiskManager reopened_manager;
    auto reopened = PageFile::Open(reopened_manager, path, file_id, FileKind::HEAP, object_id);
    if (!reopened.page_file.has_value()) {
        ADD_FAILURE() << "heap page file reopen failed";
        return;
    }
    Page read{page_id};
    ASSERT_TRUE(reopened_manager.ReadPage(page_id, read.Bytes()));
    HeapPage read_heap_page{read};
    ASSERT_TRUE(read_heap_page.Validate());

    const auto dead_slot = DecodeHeapSlotEntry(SlotBytes(read, 1));
    if (!dead_slot.entry.has_value()) {
        ADD_FAILURE() << "persisted DEAD slot did not decode";
        return;
    }
    EXPECT_EQ(dead_slot.entry->state, HeapSlotState::DEAD);
    EXPECT_FALSE(read_heap_page.TupleBytes(1).has_value());
    constexpr std::array expected_dead_bytes{std::byte{0x10}, std::byte{0x00}, std::byte{0xFF}};
    EXPECT_TRUE(std::ranges::equal(
        read.Bytes().subspan(dead_slot.entry->tuple_offset, dead_slot.entry->tuple_length),
        expected_dead_bytes));

    const auto first = read_heap_page.TupleBytes(0);
    const auto third = read_heap_page.TupleBytes(2);
    if (!first.has_value() || !third.has_value()) {
        ADD_FAILURE() << "unaffected persisted NORMAL tuple was not retrievable";
        return;
    }
    constexpr std::array expected_first{std::byte{0x01}, std::byte{0x02}};
    constexpr std::array expected_third{std::byte{0x20}};
    EXPECT_TRUE(std::ranges::equal(*first, expected_first));
    EXPECT_TRUE(std::ranges::equal(*third, expected_third));
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
        ConfigureOneNormalSlot(unsupported);
        auto slot = DecodeHeapSlotEntry(SlotBytes(unsupported, 0));
        if (!slot.entry.has_value()) {
            ADD_FAILURE() << "unsupported-state slot did not decode";
            continue;
        }
        slot.entry->state = state;
        ASSERT_TRUE(EncodeHeapSlotEntry(SlotBytes(unsupported, 0), *slot.entry));
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

TEST(HeapPageCompactionIntegrationTest, PersistsCompactedGeometryStatesAndPayloads) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("heap-compact.pages");
    constexpr FileId file_id = 52;
    constexpr std::uint64_t object_id = 7003;
    PageId page_id{};

    {
        DiskManager manager;
        auto page_file = PageFile::Create(manager,
                                          path,
                                          FileSuperblock{
                                              .file_kind = FileKind::HEAP,
                                              .file_id = file_id,
                                              .object_id = object_id,
                                              .creation_epoch = 9003,
                                          });
        if (!page_file.page_file.has_value()) {
            ADD_FAILURE() << "heap page file creation failed";
            return;
        }
        const auto allocation = page_file.page_file->AllocatePage();
        if (!allocation.page_id.has_value()) {
            ADD_FAILURE() << "heap page allocation failed";
            return;
        }
        page_id = *allocation.page_id;

        Page page{page_id};
        HeapPage heap_page{page};
        ASSERT_TRUE(heap_page.Initialize());
        constexpr std::array first{std::byte{0x01}, std::byte{0x02}};
        constexpr std::array second{
            std::byte{0x10}, std::byte{0x11}, std::byte{0x12}, std::byte{0x13}};
        constexpr std::array third{std::byte{0x20}, std::byte{0x21}, std::byte{0x22}};
        ASSERT_TRUE(heap_page.Insert(first));
        ASSERT_TRUE(heap_page.Insert(second));
        ASSERT_TRUE(heap_page.Insert(third));
        ASSERT_TRUE(heap_page.MarkDead(1));
        ASSERT_TRUE(heap_page.Compact());
        ASSERT_TRUE(manager.WritePage(page_id, page.Bytes()));
        ASSERT_TRUE(manager.SyncFile(file_id));
    }

    DiskManager reopened_manager;
    auto reopened = PageFile::Open(reopened_manager, path, file_id, FileKind::HEAP, object_id);
    if (!reopened.page_file.has_value()) {
        ADD_FAILURE() << "heap page file reopen failed";
        return;
    }
    Page read{page_id};
    ASSERT_TRUE(reopened_manager.ReadPage(page_id, read.Bytes()));
    HeapPage heap_page{read};
    ASSERT_TRUE(heap_page.Validate());

    const auto header = heap_page.Header();
    if (!header.has_value()) {
        ADD_FAILURE() << "persisted compacted header did not decode";
        return;
    }
    EXPECT_EQ(header->slot_count, std::uint16_t{3});
    EXPECT_EQ(header->lower, HEAP_PAGE_TOTAL_HEADER_SIZE + (3 * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE));
    EXPECT_EQ(header->upper, PAGE_SIZE - 5);
    EXPECT_EQ(header->free_slot_head, INVALID_SLOT_ID);

    const auto first_slot = DecodeHeapSlotEntry(SlotBytes(read, 0));
    const auto dead_slot = DecodeHeapSlotEntry(SlotBytes(read, 1));
    const auto third_slot = DecodeHeapSlotEntry(SlotBytes(read, 2));
    if (!first_slot.entry.has_value() || !dead_slot.entry.has_value() ||
        !third_slot.entry.has_value()) {
        ADD_FAILURE() << "persisted compacted slot did not decode";
        return;
    }
    EXPECT_EQ(first_slot.entry->tuple_offset, PAGE_SIZE - 2);
    EXPECT_EQ(dead_slot.entry->state, HeapSlotState::DEAD);
    EXPECT_EQ(dead_slot.entry->tuple_offset, std::uint16_t{0});
    EXPECT_EQ(dead_slot.entry->tuple_length, std::uint16_t{0});
    EXPECT_EQ(third_slot.entry->tuple_offset, PAGE_SIZE - 5);
    EXPECT_FALSE(heap_page.TupleBytes(1).has_value());

    const auto first = heap_page.TupleBytes(0);
    const auto third = heap_page.TupleBytes(2);
    if (!first.has_value() || !third.has_value()) {
        ADD_FAILURE() << "persisted live tuple was not retrievable";
        return;
    }
    constexpr std::array expected_first{std::byte{0x01}, std::byte{0x02}};
    constexpr std::array expected_third{std::byte{0x20}, std::byte{0x21}, std::byte{0x22}};
    EXPECT_TRUE(std::ranges::equal(*first, expected_first));
    EXPECT_TRUE(std::ranges::equal(*third, expected_third));
}

TEST(HeapPageIntegrationTest, PersistsAllocatedBlankHeapPageThroughDiskManager) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("heap.pages");
    constexpr FileId file_id = 41;
    DiskManager manager;
    auto page_file = PageFile::Create(manager,
                                      path,
                                      FileSuperblock{
                                          .file_kind = FileKind::HEAP,
                                          .file_id = file_id,
                                          .object_id = 7001,
                                          .creation_epoch = 9001,
                                      });
    if (!page_file.page_file.has_value()) {
        ADD_FAILURE() << "heap page file creation unexpectedly failed";
        return;
    }
    const auto allocation = page_file.page_file->AllocatePage();
    if (!allocation.page_id.has_value()) {
        ADD_FAILURE() << "heap page allocation unexpectedly failed";
        return;
    }

    Page written{*allocation.page_id};
    HeapPage written_heap_page{written};
    ASSERT_TRUE(written_heap_page.Initialize());
    constexpr std::array first{std::byte{0x01}, std::byte{0x00}, std::byte{0xFF}};
    constexpr std::array second{std::byte{0x10}, std::byte{0x20}};
    ASSERT_TRUE(written_heap_page.Insert(first));
    ASSERT_TRUE(written_heap_page.Insert(second));
    ASSERT_TRUE(manager.WritePage(written.Id(), written.Bytes()));

    Page read{*allocation.page_id};
    ASSERT_TRUE(manager.ReadPage(read.Id(), read.Bytes()));
    HeapPage read_heap_page{read};
    const auto validation = read_heap_page.Validate();
    EXPECT_TRUE(validation);
    EXPECT_TRUE(std::ranges::equal(read.Bytes(), written.Bytes()));
    const auto stored_first = read_heap_page.TupleBytes(0);
    const auto stored_second = read_heap_page.TupleBytes(1);
    if (!stored_first.has_value() || !stored_second.has_value()) {
        ADD_FAILURE() << "persisted tuples were not retrievable";
        return;
    }
    EXPECT_TRUE(std::ranges::equal(*stored_first, first));
    EXPECT_TRUE(std::ranges::equal(*stored_second, second));
}

} // namespace
} // namespace dblusblus
