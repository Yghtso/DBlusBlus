#include "common/types.h"

#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <limits>
#include <type_traits>

namespace dblusblus {
namespace {

static_assert(std::is_same_v<FileId, std::uint32_t>);
static_assert(std::is_same_v<PageNo, std::uint64_t>);
static_assert(std::is_same_v<SlotId, std::uint16_t>);
static_assert(std::is_same_v<TxnId, std::uint64_t>);
static_assert(std::is_same_v<CommandId, std::uint32_t>);
static_assert(std::is_same_v<Lsn, std::uint64_t>);
static_assert(std::is_same_v<TableId, std::uint64_t>);
static_assert(std::is_same_v<IndexId, std::uint64_t>);
static_assert(std::is_same_v<SchemaVer, std::uint32_t>);
static_assert(std::is_trivially_copyable_v<PageId>);
static_assert(std::is_trivially_copyable_v<Rid>);

TEST(IdentifierTypesTest, SentinelsHaveLockedValues) {
    EXPECT_EQ(INVALID_FILE_ID, FileId{0});
    EXPECT_EQ(INVALID_PAGE_NO, std::numeric_limits<PageNo>::max());
    EXPECT_EQ(INVALID_SLOT_ID, std::numeric_limits<SlotId>::max());
    EXPECT_EQ(INVALID_TXN_ID, TxnId{0});
    EXPECT_EQ(FROZEN_TXN_ID, TxnId{1});
    EXPECT_EQ(FIRST_NORMAL_TXN_ID, TxnId{2});
    EXPECT_EQ(INVALID_LSN, Lsn{0});
}

TEST(IdentifierTypesTest, ScalarValueInitializationUsesZero) {
    EXPECT_EQ(FileId{}, FileId{0});
    EXPECT_EQ(PageNo{}, PageNo{0});
    EXPECT_EQ(SlotId{}, SlotId{0});
    EXPECT_EQ(TxnId{}, TxnId{0});
    EXPECT_EQ(CommandId{}, CommandId{0});
    EXPECT_EQ(Lsn{}, Lsn{0});
    EXPECT_EQ(TableId{}, TableId{0});
    EXPECT_EQ(IndexId{}, IndexId{0});
    EXPECT_EQ(SchemaVer{}, SchemaVer{0});
}

TEST(PageIdTest, DefaultsToInvalidFields) {
    const PageId page_id;

    EXPECT_EQ(page_id.file_id, INVALID_FILE_ID);
    EXPECT_EQ(page_id.page_no, INVALID_PAGE_NO);
}

TEST(PageIdTest, PreservesExplicitFieldsAndComparesLexicographically) {
    const PageId page{.file_id = FileId{7}, .page_no = PageNo{42}};
    const PageId equal_page{.file_id = FileId{7}, .page_no = PageNo{42}};

    EXPECT_EQ(page.file_id, FileId{7});
    EXPECT_EQ(page.page_no, PageNo{42});
    EXPECT_EQ(page, equal_page);
    EXPECT_LT(page, (PageId{.file_id = FileId{7}, .page_no = PageNo{43}}));
    EXPECT_LT(page, (PageId{.file_id = FileId{8}, .page_no = PageNo{1}}));
    EXPECT_EQ(std::hash<PageId>{}(page), std::hash<PageId>{}(equal_page));
}

TEST(RidTest, DefaultsToInvalidFields) {
    const Rid rid;

    EXPECT_EQ(rid.page, PageId{});
    EXPECT_EQ(rid.slot, INVALID_SLOT_ID);
}

TEST(RidTest, PreservesPhysicalAddressAndComparesLexicographically) {
    const Rid rid{
        .page = PageId{.file_id = FileId{3}, .page_no = PageNo{91}},
        .slot = SlotId{7},
    };

    EXPECT_EQ(rid.page.file_id, FileId{3});
    EXPECT_EQ(rid.page.page_no, PageNo{91});
    EXPECT_EQ(rid.slot, SlotId{7});
    EXPECT_EQ(rid,
              (Rid{
                  .page = PageId{.file_id = FileId{3}, .page_no = PageNo{91}},
                  .slot = SlotId{7},
              }));
    EXPECT_LT(rid,
              (Rid{
                  .page = PageId{.file_id = FileId{3}, .page_no = PageNo{91}},
                  .slot = SlotId{8},
              }));
    EXPECT_LT(rid,
              (Rid{
                  .page = PageId{.file_id = FileId{4}, .page_no = PageNo{1}},
                  .slot = SlotId{1},
              }));
}

} // namespace
} // namespace dblusblus
