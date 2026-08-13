#include "storage/disk_manager.h"
#include "storage/page.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>
#include <span>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unistd.h>

namespace dblusblus {
namespace {

static_assert(sizeof(Page::ByteStorage) == PAGE_SIZE);
static_assert(sizeof(Page) > PAGE_SIZE);
static_assert(!std::is_polymorphic_v<Page>);

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        std::array path_template{
            '/', 't', 'm', 'p', '/', 'd', 'b', 'l', 'u', 's', 'b', 'l', 'u',  's',
            '-', 'p', 'a', 'g', 'e', '-', 'X', 'X', 'X', 'X', 'X', 'X', '\0',
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

[[nodiscard]] CommonPageHeader ExampleHeader(PageNo page_no) {
    return CommonPageHeader{
        .page_type = PageType::BTREE_LEAF,
        .format_version = 3,
        .flags = 0xA5F00F5AU,
        .page_lsn = 19,
        .checksum_crc32c = 0,
        .header_size = COMMON_PAGE_HEADER_ENCODED_SIZE,
        .reserved16 = 0,
        .page_no = page_no,
    };
}

TEST(PageTest, OwnsExactlyOneDeterministicallyZeroedByteBuffer) {
    const Page page;

    EXPECT_EQ(page.Id(), PageId{});
    EXPECT_EQ(page.Bytes().size(), PAGE_SIZE);
    EXPECT_TRUE(
        std::ranges::all_of(page.Bytes(), [](std::byte value) { return value == std::byte{0}; }));
}

TEST(PageTest, ExposesContiguousMutableAndConstByteSpans) {
    Page page{PageId{.file_id = 7, .page_no = 11}};
    auto mutable_bytes = page.Bytes();
    mutable_bytes[37] = std::byte{0xA5};

    const Page& const_page = page;
    const auto const_bytes = const_page.Bytes();
    EXPECT_EQ(const_bytes.data(), mutable_bytes.data());
    EXPECT_EQ(const_bytes[37], std::byte{0xA5});
    EXPECT_EQ(page.Id(), (PageId{.file_id = 7, .page_no = 11}));
}

TEST(PageTest, InitializationZerosThePageAndEncodesTheSuppliedHeader) {
    constexpr PageNo page_no = 0x0102030405060708ULL;
    Page page{PageId{.file_id = 23, .page_no = page_no}};
    std::ranges::fill(page.Bytes(), std::byte{0xA5});
    const auto expected = ExampleHeader(page_no);

    ASSERT_TRUE(page.Initialize(expected));
    const auto decoded = page.DecodeHeader();
    if (!decoded.has_value()) {
        ADD_FAILURE() << "page header decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(*decoded, expected);
    EXPECT_EQ(page.Bytes()[0], std::byte{0x04});
    EXPECT_EQ(page.Bytes()[1], std::byte{0x00});
    EXPECT_EQ(page.Bytes()[24], std::byte{0x08});
    EXPECT_EQ(page.Bytes()[31], std::byte{0x01});
    EXPECT_TRUE(std::ranges::all_of(page.Bytes().subspan(COMMON_PAGE_HEADER_ENCODED_SIZE),
                                    [](std::byte value) { return value == std::byte{0}; }));
}

TEST(PageTest, HeaderUpdatesDoNotModifyPageTypeSpecificBytes) {
    Page page{PageId{.file_id = 29, .page_no = 5}};
    ASSERT_TRUE(page.Initialize(ExampleHeader(5)));
    page.Bytes()[COMMON_PAGE_HEADER_ENCODED_SIZE + 17] = std::byte{0x5A};

    auto updated = ExampleHeader(5);
    updated.flags = 0x12345678U;
    updated.checksum_crc32c = 0xABCDEF01U;
    ASSERT_TRUE(page.WriteHeader(updated));

    const auto decoded = page.DecodeHeader();
    if (!decoded.has_value()) {
        ADD_FAILURE() << "page header decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(*decoded, updated);
    EXPECT_EQ(page.Bytes()[COMMON_PAGE_HEADER_ENCODED_SIZE + 17], std::byte{0x5A});
}

TEST(PageTest, ValidatesExpectedPageTypeAndStoredPageNumber) {
    Page page{PageId{.file_id = 31, .page_no = 7}};
    ASSERT_TRUE(page.Initialize(ExampleHeader(7)));

    const auto valid = page.ValidateHeader(PageType::BTREE_LEAF);
    ASSERT_TRUE(valid);
    if (!valid.header.has_value()) {
        ADD_FAILURE() << "validated page did not return its decoded header";
        return;
    }
    EXPECT_EQ(*valid.header, ExampleHeader(7));

    const auto wrong_type = page.ValidateHeader(PageType::HEAP_DATA);
    EXPECT_FALSE(wrong_type);
    EXPECT_EQ(wrong_type.error, PageHeaderValidationError::UNEXPECTED_PAGE_TYPE);

    Page wrong_identity{PageId{.file_id = 31, .page_no = 8}};
    ASSERT_TRUE(wrong_identity.Initialize(ExampleHeader(7)));
    const auto wrong_page_number = wrong_identity.ValidateHeader(PageType::BTREE_LEAF);
    EXPECT_FALSE(wrong_page_number);
    EXPECT_EQ(wrong_page_number.error, PageHeaderValidationError::UNEXPECTED_PAGE_NUMBER);
}

TEST(PageTest, RejectsCorruptHeaderIdentity) {
    Page page{PageId{.file_id = 0xAABBCCDDU, .page_no = 9}};
    ASSERT_TRUE(page.Initialize(ExampleHeader(9)));

    page.Bytes()[0] = std::byte{0xFF};
    page.Bytes()[1] = std::byte{0xFF};
    const auto corrupt_type = page.ValidateHeader(PageType::BTREE_LEAF);
    EXPECT_FALSE(corrupt_type);
    EXPECT_EQ(corrupt_type.error, PageHeaderValidationError::UNEXPECTED_PAGE_TYPE);
}

TEST(PageTest, InteroperatesWithDiskManagerThroughExactPageSpans) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("raw-page.pages");
    constexpr PageId page_id{.file_id = 41, .page_no = 0};
    DiskManager manager;
    ASSERT_TRUE(manager.CreateFile(page_id.file_id, path));
    const auto allocated = manager.ExtendFile(page_id.file_id);
    ASSERT_TRUE(allocated);
    ASSERT_EQ(allocated.value, page_id.page_no);

    Page written{page_id};
    ASSERT_TRUE(written.Initialize(ExampleHeader(page_id.page_no)));
    written.Bytes()[PAGE_SIZE - 1] = std::byte{0x7E};
    ASSERT_TRUE(manager.WritePage(written.Id(), written.Bytes()));

    Page read{page_id};
    ASSERT_TRUE(manager.ReadPage(read.Id(), read.Bytes()));
    EXPECT_TRUE(std::ranges::equal(read.Bytes(), written.Bytes()));
    EXPECT_TRUE(read.ValidateHeader(PageType::BTREE_LEAF));
}

} // namespace
} // namespace dblusblus
