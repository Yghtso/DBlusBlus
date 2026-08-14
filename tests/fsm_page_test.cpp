#include "common/encoding.h"
#include "common/file_superblock.h"
#include "common/page_header.h"
#include "storage/disk_manager.h"
#include "storage/fsm_page.h"
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
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace dblusblus {
namespace {

static_assert(FSM_PAGE_HEADER_OFFSET == 32);
static_assert(FSM_PAGE_HEADER_ENCODED_SIZE == 16);
static_assert(FSM_PAGE_TOTAL_HEADER_SIZE == 48);
static_assert(FSM_PAGE_ENTRIES_OFFSET == 48);
static_assert(FSM_PAGE_ENTRY_CAPACITY == 8144);
static_assert(FSM_MAX_CONTIGUOUS_FREE_BYTES == 8144);
static_assert(FSM_MAX_USABLE_INSERTION_BYTES == 8135);

template <typename Value>
[[nodiscard]] const Value* RequireOptional(const std::optional<Value>& optional,
                                           std::string_view description) {
    if (!optional.has_value()) {
        ADD_FAILURE() << description << " unexpectedly missing";
        return nullptr;
    }
    return std::addressof(*optional);
}

template <typename Value>
[[nodiscard]] Value* RequireOptional(std::optional<Value>& optional, std::string_view description) {
    if (!optional.has_value()) {
        ADD_FAILURE() << description << " unexpectedly missing";
        return nullptr;
    }
    return std::addressof(*optional);
}

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        std::array path_template{
            '/', 't', 'm', 'p', '/', 'd', 'b', 'l', 'u', 's', 'b', 'l', 'u',
            's', '-', 'f', 's', 'm', '-', 'X', 'X', 'X', 'X', 'X', 'X', '\0',
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

[[nodiscard]] Page InitializedFsmPage(std::uint16_t entry_count = 3) {
    Page page{PageId{.file_id = 17, .page_no = 1}};
    FsmPage fsm_page{page};
    if (!fsm_page.Initialize({.entry_count = entry_count})) {
        ADD_FAILURE() << "FSM page initialization unexpectedly failed";
    }
    return page;
}

void WriteCommonHeader(Page& page, const CommonPageHeader& header) {
    ASSERT_TRUE(page.WriteHeader(header));
}

void WriteFsmHeader(Page& page, const FsmPageHeader& header) {
    ASSERT_TRUE(EncodeFsmPageHeader(
        page.Bytes().subspan(FSM_PAGE_HEADER_OFFSET, FSM_PAGE_HEADER_ENCODED_SIZE), header));
}

[[nodiscard]] CommonPageHeader RequireCommonHeader(const Page& page) {
    const auto header = page.DecodeHeader();
    if (!header.has_value()) {
        ADD_FAILURE() << "common page header unexpectedly failed to decode";
        return {};
    }
    return *header;
}

[[nodiscard]] FsmPageHeader RequireFsmHeader(Page& page) {
    const auto header = FsmPage{page}.Header();
    if (!header.has_value()) {
        ADD_FAILURE() << "FSM-specific header unexpectedly failed to decode";
        return {};
    }
    return *header;
}

[[nodiscard]] Page::ByteStorage CopyPageBytes(const Page& page) {
    Page::ByteStorage copy{};
    std::ranges::copy(page.Bytes(), copy.begin());
    return copy;
}

TEST(FsmCategoryTest, PinsSlotAwareIntegerMappingAndInverseBoundaries) {
    EXPECT_EQ(FsmCategoryForFreeBytes(0), std::uint8_t{0});
    EXPECT_EQ(FsmCategoryForFreeBytes(1), std::uint8_t{0});
    EXPECT_EQ(FsmCategoryForFreeBytes(HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE), std::uint8_t{0});
    EXPECT_EQ(FsmCategoryForFreeBytes(HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE + 1U), std::uint8_t{0});
    EXPECT_EQ(FsmCategoryForFreeBytes(39), std::uint8_t{0});
    EXPECT_EQ(FsmCategoryForFreeBytes(40), std::uint8_t{1});
    EXPECT_EQ(FsmCategoryForFreeBytes(4075), std::uint8_t{127});
    EXPECT_EQ(FsmCategoryForFreeBytes(8142), std::uint8_t{254});
    EXPECT_EQ(FsmCategoryForFreeBytes(8143), std::uint8_t{255});
    EXPECT_EQ(FsmCategoryForFreeBytes(FSM_MAX_CONTIGUOUS_FREE_BYTES), std::uint8_t{255});
    EXPECT_EQ(FsmCategoryForFreeBytes(FSM_MAX_CONTIGUOUS_FREE_BYTES + 1000U), std::uint8_t{255});

    EXPECT_EQ(FsmCategoryMinimumUsableBytes(0), 0U);
    EXPECT_EQ(FsmCategoryMinimumUsableBytes(1), 32U);
    EXPECT_EQ(FsmCategoryMinimumUsableBytes(2), 64U);
    EXPECT_EQ(FsmCategoryMinimumUsableBytes(127), 4052U);
    EXPECT_EQ(FsmCategoryMinimumUsableBytes(128), 4084U);
    EXPECT_EQ(FsmCategoryMinimumUsableBytes(254), 8104U);
    EXPECT_EQ(FsmCategoryMinimumUsableBytes(255), FSM_MAX_USABLE_INSERTION_BYTES);
}

TEST(FsmCategoryTest, IsMonotonicAndNeverOverstatesItsLowerBoundAcrossDomain) {
    std::uint8_t previous_category = 0;
    for (std::size_t free_bytes = 0; free_bytes <= FSM_MAX_CONTIGUOUS_FREE_BYTES; ++free_bytes) {
        const auto category = FsmCategoryForFreeBytes(free_bytes);
        EXPECT_GE(category, previous_category) << "free bytes " << free_bytes;

        const std::size_t usable_bytes =
            free_bytes <= HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE
                ? 0
                : std::min(free_bytes - HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE,
                           FSM_MAX_USABLE_INSERTION_BYTES);
        EXPECT_LE(FsmCategoryMinimumUsableBytes(category), usable_bytes)
            << "free bytes " << free_bytes;
        if (category < std::numeric_limits<std::uint8_t>::max()) {
            EXPECT_LT(usable_bytes,
                      FsmCategoryMinimumUsableBytes(static_cast<std::uint8_t>(category + 1U)))
                << "free bytes " << free_bytes;
        }
        previous_category = category;
    }
}

TEST(FsmPageMappingTest, PinsFlatPageAndEntryBoundaries) {
    struct MappingCase {
        PageNo heap_page_no;
        FsmEntryLocation expected;
    };
    constexpr std::array cases{
        MappingCase{.heap_page_no = 1, .expected = {.fsm_page_no = 1, .entry_index = 0}},
        MappingCase{.heap_page_no = 8144, .expected = {.fsm_page_no = 1, .entry_index = 8143}},
        MappingCase{.heap_page_no = 8145, .expected = {.fsm_page_no = 2, .entry_index = 0}},
        MappingCase{.heap_page_no = 16288, .expected = {.fsm_page_no = 2, .entry_index = 8143}},
        MappingCase{.heap_page_no = 16289, .expected = {.fsm_page_no = 3, .entry_index = 0}},
        MappingCase{.heap_page_no = 100000, .expected = {.fsm_page_no = 13, .entry_index = 2271}},
    };

    for (const auto& test_case : cases) {
        const auto mapping = FsmLocationForHeapPage(test_case.heap_page_no);
        const auto* location = RequireOptional(mapping.location, "FSM entry location");
        if (location == nullptr) {
            continue;
        }
        EXPECT_EQ(mapping.error, FsmPageMappingError::NONE);
        EXPECT_EQ(*location, test_case.expected);
    }
}

TEST(FsmPageMappingTest, RejectsNonDataPagesAndHandlesPageNumberLimitSafely) {
    EXPECT_EQ(FsmLocationForHeapPage(0).error, FsmPageMappingError::INVALID_HEAP_PAGE_NUMBER);
    EXPECT_EQ(FsmLocationForHeapPage(INVALID_PAGE_NO).error,
              FsmPageMappingError::INVALID_HEAP_PAGE_NUMBER);

    constexpr PageNo maximum_valid_heap_page = std::numeric_limits<PageNo>::max() - 1U;
    const auto mapping = FsmLocationForHeapPage(maximum_valid_heap_page);
    const auto* location = RequireOptional(mapping.location, "maximum heap page FSM location");
    if (location == nullptr) {
        return;
    }
    EXPECT_EQ(location->fsm_page_no, PageNo{2265071718284572ULL});
    EXPECT_EQ(location->entry_index, std::uint16_t{5389});
}

TEST(FsmPageHeaderCodecTest, EmitsExactSixteenByteLittleEndianLayout) {
    const FsmPageHeader header{
        .first_heap_page_no = PageNo{0x0807060504030201ULL},
        .entry_count = std::uint16_t{0x0A09U},
        .reserved16 = std::uint16_t{0x0C0BU},
        .reserved32 = std::uint32_t{0x100F0E0DU},
    };
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
    std::array<std::byte, FSM_PAGE_HEADER_ENCODED_SIZE> encoded{};

    ASSERT_TRUE(EncodeFsmPageHeader(encoded, header));
    EXPECT_EQ(encoded, expected);
    const auto decoded = DecodeFsmPageHeader(encoded);
    const auto* decoded_header = RequireOptional(decoded, "FSM-specific header");
    if (decoded_header == nullptr) {
        return;
    }
    EXPECT_EQ(*decoded_header, header);
}

TEST(FsmPageHeaderCodecTest, SupportsUnalignedAndAtomicUndersizedBuffers) {
    constexpr auto padding = std::byte{0xA5};
    const FsmPageHeader header{.first_heap_page_no = 8145, .entry_count = 7};
    std::array<std::byte, FSM_PAGE_HEADER_ENCODED_SIZE + 2> buffer{};
    buffer.fill(padding);
    auto unaligned = std::span{buffer}.subspan(1, FSM_PAGE_HEADER_ENCODED_SIZE);

    ASSERT_TRUE(EncodeFsmPageHeader(unaligned, header));
    EXPECT_EQ(buffer.front(), padding);
    EXPECT_EQ(buffer.back(), padding);
    EXPECT_EQ(DecodeFsmPageHeader(unaligned), std::optional{header});

    std::array<std::byte, FSM_PAGE_HEADER_ENCODED_SIZE> undersized_buffer{};
    undersized_buffer.fill(padding);
    const auto original = undersized_buffer;
    const auto undersized = std::span{undersized_buffer}.first(FSM_PAGE_HEADER_ENCODED_SIZE - 1U);
    EXPECT_FALSE(EncodeFsmPageHeader(undersized, header));
    EXPECT_EQ(undersized_buffer, original);
    EXPECT_FALSE(DecodeFsmPageHeader(undersized).has_value());
}

TEST(FsmPageTest, InitializesDeterministicBlankPageAndDerivesRangeMetadata) {
    Page page{PageId{.file_id = 19, .page_no = 2}};
    std::ranges::fill(page.Bytes(), std::byte{0xA5});
    FsmPage fsm_page{page};

    ASSERT_TRUE(fsm_page.Initialize({
        .entry_count = 3,
        .flags = std::uint32_t{0x1234U},
        .page_lsn = Lsn{27},
    }));
    const auto common_header = page.DecodeHeader();
    const auto* common = RequireOptional(common_header, "initialized FSM common header");
    if (common == nullptr) {
        return;
    }
    EXPECT_EQ(common->page_type, PageType::FSM_DATA);
    EXPECT_EQ(common->format_version, FSM_PAGE_FORMAT_VERSION);
    EXPECT_EQ(common->flags, std::uint32_t{0x1234U});
    EXPECT_EQ(common->page_lsn, Lsn{27});
    EXPECT_EQ(common->checksum_crc32c, std::uint32_t{0});
    EXPECT_EQ(common->header_size, FSM_PAGE_TOTAL_HEADER_SIZE);
    EXPECT_EQ(common->reserved16, std::uint16_t{0});
    EXPECT_EQ(common->page_no, PageNo{2});

    const auto header = fsm_page.Header();
    const auto* decoded_header = RequireOptional(header, "initialized FSM header");
    if (decoded_header == nullptr) {
        return;
    }
    EXPECT_EQ(*decoded_header, (FsmPageHeader{.first_heap_page_no = 8145, .entry_count = 3}));
    EXPECT_TRUE(std::ranges::all_of(page.Bytes().subspan(FSM_PAGE_ENTRIES_OFFSET),
                                    [](std::byte value) { return value == std::byte{0}; }));
    EXPECT_TRUE(fsm_page.Validate());
}

TEST(FsmPageTest, ReadsAndUpdatesOnlyInitializedOneByteEntries) {
    Page page = InitializedFsmPage();
    FsmPage fsm_page{page};

    ASSERT_TRUE(fsm_page.SetCategory(0, 0));
    ASSERT_TRUE(fsm_page.SetCategory(1, 127));
    const auto before_last_update = page.Bytes();
    Page::ByteStorage before{};
    std::ranges::copy(before_last_update, before.begin());
    ASSERT_TRUE(fsm_page.SetCategory(2, 255));

    for (std::size_t index = 0; index < page.Bytes().size(); ++index) {
        if (index == FSM_PAGE_ENTRIES_OFFSET + 2U) {
            EXPECT_EQ(page.Bytes()[index], std::byte{0xFF});
        } else {
            EXPECT_EQ(page.Bytes()[index], before[index]);
        }
    }
    EXPECT_EQ(fsm_page.GetCategory(0).category, std::optional<std::uint8_t>{0});
    EXPECT_EQ(fsm_page.GetCategory(1).category, std::optional<std::uint8_t>{127});
    EXPECT_EQ(fsm_page.GetCategory(2).category, std::optional<std::uint8_t>{255});

    const Page::ByteStorage valid_bytes = CopyPageBytes(page);
    EXPECT_EQ(fsm_page.GetCategory(3).error, FsmPageEntryError::ENTRY_OUT_OF_RANGE);
    EXPECT_EQ(fsm_page.SetCategory(3, 1).error, FsmPageEntryError::ENTRY_OUT_OF_RANGE);
    EXPECT_TRUE(std::ranges::equal(page.Bytes(), valid_bytes));
}

TEST(FsmPageTest, InitializationFailuresLeavePageUnchanged) {
    const auto expect_unchanged = [](Page& page, FsmPageInitializeResult result) {
        EXPECT_FALSE(result);
        EXPECT_TRUE(std::ranges::all_of(page.Bytes(),
                                        [](std::byte value) { return value == std::byte{0xA5}; }));
    };

    Page page_zero{PageId{.file_id = 23, .page_no = 0}};
    std::ranges::fill(page_zero.Bytes(), std::byte{0xA5});
    FsmPage page_zero_fsm{page_zero};
    const auto page_zero_result = page_zero_fsm.Initialize();
    EXPECT_EQ(page_zero_result.error, FsmPageInitializeError::INVALID_FSM_PAGE_NUMBER);
    expect_unchanged(page_zero, page_zero_result);

    Page oversized_count{PageId{.file_id = 23, .page_no = 1}};
    std::ranges::fill(oversized_count.Bytes(), std::byte{0xA5});
    FsmPage oversized_count_fsm{oversized_count};
    const auto count_result = oversized_count_fsm.Initialize({
        .entry_count = static_cast<std::uint16_t>(FSM_PAGE_ENTRY_CAPACITY + 1U),
    });
    EXPECT_EQ(count_result.error, FsmPageInitializeError::ENTRY_COUNT_OUT_OF_RANGE);
    expect_unchanged(oversized_count, count_result);

    Page overflow{PageId{.file_id = 23, .page_no = std::numeric_limits<PageNo>::max() - 1U}};
    std::ranges::fill(overflow.Bytes(), std::byte{0xA5});
    FsmPage overflow_fsm{overflow};
    const auto overflow_result = overflow_fsm.Initialize();
    EXPECT_EQ(overflow_result.error, FsmPageInitializeError::HEAP_PAGE_RANGE_OVERFLOW);
    expect_unchanged(overflow, overflow_result);
}

TEST(FsmPageValidationTest, RejectsCommonAndSpecificHeaderCorruption) {
    {
        Page page = InitializedFsmPage();
        auto header = RequireCommonHeader(page);
        header.page_type = PageType::HEAP_DATA;
        WriteCommonHeader(page, header);
        EXPECT_EQ(FsmPage{page}.Validate().error, FsmPageValidationError::WRONG_PAGE_TYPE);
    }
    {
        Page page = InitializedFsmPage();
        auto header = RequireCommonHeader(page);
        header.page_no = 9;
        WriteCommonHeader(page, header);
        EXPECT_EQ(FsmPage{page}.Validate().error, FsmPageValidationError::WRONG_PAGE_NUMBER);
    }
    {
        Page page = InitializedFsmPage();
        auto header = RequireCommonHeader(page);
        header.header_size = FSM_PAGE_TOTAL_HEADER_SIZE + 1U;
        WriteCommonHeader(page, header);
        EXPECT_EQ(FsmPage{page}.Validate().error, FsmPageValidationError::WRONG_HEADER_SIZE);
    }
    {
        Page page = InitializedFsmPage();
        auto header = RequireCommonHeader(page);
        header.format_version = FSM_PAGE_FORMAT_VERSION + 1U;
        WriteCommonHeader(page, header);
        EXPECT_EQ(FsmPage{page}.Validate().error,
                  FsmPageValidationError::UNSUPPORTED_FORMAT_VERSION);
    }
    {
        Page page = InitializedFsmPage();
        auto header = RequireCommonHeader(page);
        header.reserved16 = 1;
        WriteCommonHeader(page, header);
        EXPECT_EQ(FsmPage{page}.Validate().error, FsmPageValidationError::NONZERO_COMMON_RESERVED);
    }
    {
        Page page = InitializedFsmPage();
        auto header = RequireFsmHeader(page);
        header.first_heap_page_no = 2;
        WriteFsmHeader(page, header);
        EXPECT_EQ(FsmPage{page}.Validate().error, FsmPageValidationError::FIRST_HEAP_PAGE_MISMATCH);
    }
    {
        Page page = InitializedFsmPage();
        auto header = RequireFsmHeader(page);
        header.entry_count = static_cast<std::uint16_t>(FSM_PAGE_ENTRY_CAPACITY + 1U);
        WriteFsmHeader(page, header);
        EXPECT_EQ(FsmPage{page}.Validate().error, FsmPageValidationError::ENTRY_COUNT_OUT_OF_RANGE);
    }
    {
        Page page = InitializedFsmPage();
        auto header = RequireFsmHeader(page);
        header.reserved32 = 1;
        WriteFsmHeader(page, header);
        EXPECT_EQ(FsmPage{page}.Validate().error, FsmPageValidationError::NONZERO_FSM_RESERVED);
    }
}

TEST(FsmPageValidationTest, RequiresUninitializedEntrySuffixToRemainZero) {
    Page page = InitializedFsmPage(3);
    page.Bytes()[FSM_PAGE_ENTRIES_OFFSET + 3U] = std::byte{0x01};
    FsmPage fsm_page{page};

    const auto validation = fsm_page.Validate();
    EXPECT_EQ(validation.error, FsmPageValidationError::NONZERO_UNINITIALIZED_ENTRY);
    EXPECT_EQ(validation.entry_index, 3U);

    const Page::ByteStorage corrupt_bytes = CopyPageBytes(page);
    const auto update = fsm_page.SetCategory(0, 7);
    EXPECT_EQ(update.error, FsmPageEntryError::PAGE_INVALID);
    EXPECT_EQ(update.page_error, FsmPageValidationError::NONZERO_UNINITIALIZED_ENTRY);
    EXPECT_TRUE(std::ranges::equal(page.Bytes(), corrupt_bytes));
}

TEST(FsmPageValidationTest, EnforcesRepresentableHeapRangeAtPageNumberLimit) {
    constexpr PageNo maximum_valid_heap_page = std::numeric_limits<PageNo>::max() - 1U;
    const auto mapping = FsmLocationForHeapPage(maximum_valid_heap_page);
    const auto* location = RequireOptional(mapping.location, "last FSM page location");
    if (location == nullptr) {
        return;
    }

    Page page{PageId{.file_id = 29, .page_no = location->fsm_page_no}};
    FsmPage fsm_page{page};
    ASSERT_TRUE(fsm_page.Initialize({.entry_count = 5390}));
    EXPECT_TRUE(fsm_page.Validate());

    Page overflow_page{PageId{.file_id = 29, .page_no = location->fsm_page_no}};
    std::ranges::fill(overflow_page.Bytes(), std::byte{0xA5});
    FsmPage overflow_fsm_page{overflow_page};
    const auto overflow = overflow_fsm_page.Initialize({.entry_count = 5391});
    EXPECT_EQ(overflow.error, FsmPageInitializeError::HEAP_PAGE_RANGE_OVERFLOW);
    EXPECT_TRUE(std::ranges::all_of(overflow_page.Bytes(),
                                    [](std::byte value) { return value == std::byte{0xA5}; }));
}

TEST(FsmPagePersistenceTest, SurvivesPageFileWriteReopenAndValidation) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("table.fsm");
    constexpr FileId file_id = 81;
    constexpr std::uint64_t table_id = 1801;
    const FileSuperblock superblock{
        .file_kind = FileKind::FSM,
        .file_id = file_id,
        .object_id = table_id,
        .creation_epoch = 2801,
    };

    DiskManager manager;
    PageId fsm_page_id{};
    {
        auto created = PageFile::Create(manager, path, superblock);
        auto* page_file = RequireOptional(created.page_file, "created FSM PageFile");
        if (page_file == nullptr) {
            return;
        }
        const auto allocation = page_file->AllocatePage();
        const auto* allocated_page = RequireOptional(allocation.page_id, "allocated FSM page");
        if (allocated_page == nullptr) {
            return;
        }
        fsm_page_id = *allocated_page;
        ASSERT_EQ(fsm_page_id.page_no, PageNo{1});

        Page page{fsm_page_id};
        FsmPage fsm_page{page};
        ASSERT_TRUE(fsm_page.Initialize({.entry_count = 3}));
        ASSERT_TRUE(fsm_page.SetCategory(0, 0));
        ASSERT_TRUE(fsm_page.SetCategory(1, 127));
        ASSERT_TRUE(fsm_page.SetCategory(2, 255));
        ASSERT_TRUE(manager.WritePage(page.Id(), page.Bytes()));
        ASSERT_TRUE(manager.SyncFile(file_id));
    }

    auto reopened = PageFile::Open(manager, path, file_id, FileKind::FSM, table_id);
    ASSERT_TRUE(reopened.page_file.has_value());
    Page page{fsm_page_id};
    ASSERT_TRUE(manager.ReadPage(page.Id(), page.Bytes()));
    FsmPage fsm_page{page};
    EXPECT_TRUE(fsm_page.Validate());
    EXPECT_EQ(fsm_page.GetCategory(0).category, std::optional<std::uint8_t>{0});
    EXPECT_EQ(fsm_page.GetCategory(1).category, std::optional<std::uint8_t>{127});
    EXPECT_EQ(fsm_page.GetCategory(2).category, std::optional<std::uint8_t>{255});
}

} // namespace
} // namespace dblusblus
