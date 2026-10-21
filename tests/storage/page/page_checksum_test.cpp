#include "common/crc32c.h"
#include "storage/page/page.h"
#include "storage/page/page_checksum.h"

#include <array>
#include <cstddef>
#include <gtest/gtest.h>

namespace dblusblus {
namespace {

TEST(PageChecksumTest, TreatsChecksumFieldAsZeroAndStoresLittleEndian) {
    Page page{PageId{.file_id = 1, .page_no = 1}};
    page.Bytes()[100] = std::byte{0x42};
    std::array<std::byte, PAGE_SIZE> zeroed{};
    std::ranges::copy(page.Bytes(), zeroed.begin());
    const auto expected = Crc32c(zeroed);

    page.Bytes()[16] = std::byte{0xFF}; // stale checksum bytes do not change the result
    EXPECT_EQ(ComputePageChecksum(page.Bytes()), expected);

    FinalizePageChecksum(page.Bytes());
    EXPECT_EQ(page.Bytes()[16], static_cast<std::byte>(expected & 0xFFU));
    EXPECT_EQ(page.Bytes()[19], static_cast<std::byte>((expected >> 24U) & 0xFFU));
    EXPECT_TRUE(VerifyPageChecksum(page.Bytes()));
}

TEST(PageChecksumTest, DetectsAnyChangedByte) {
    Page page{PageId{.file_id = 1, .page_no = 1}};
    FinalizePageChecksum(page.Bytes());
    ASSERT_TRUE(VerifyPageChecksum(page.Bytes()));
    page.Bytes()[PAGE_SIZE - 1U] = std::byte{1};
    EXPECT_FALSE(VerifyPageChecksum(page.Bytes()));
}

TEST(PageChecksumTest, AllZeroPageDoesNotVerify) {
    const Page page{};
    EXPECT_FALSE(VerifyPageChecksum(page.Bytes()));
}

} // namespace
} // namespace dblusblus
