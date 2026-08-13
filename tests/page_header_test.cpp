#include "common/page_header.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <type_traits>

namespace dblusblus {
namespace {

static_assert(COMMON_PAGE_HEADER_ENCODED_SIZE == 32);
static_assert(std::is_same_v<std::underlying_type_t<PageType>, std::uint16_t>);

void ExpectHeaderRoundTrip(const CommonPageHeader& expected) {
    std::array<std::byte, COMMON_PAGE_HEADER_ENCODED_SIZE> buffer{};

    ASSERT_TRUE(EncodeCommonPageHeader(buffer, expected));
    const auto decoded = DecodeCommonPageHeader(buffer);
    if (!decoded.has_value()) {
        ADD_FAILURE() << "common page header decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(*decoded, expected);
}

TEST(CommonPageHeaderCodecTest, UsesLockedEncodedSize) {
    EXPECT_EQ(COMMON_PAGE_HEADER_ENCODED_SIZE, std::size_t{32});
}

TEST(CommonPageHeaderCodecTest, EmitsExactPersistedLayout) {
    const CommonPageHeader header{
        .page_type = PageType::BTREE_LEAF,
        .format_version = std::uint16_t{0x0201U},
        .flags = std::uint32_t{0x06050403U},
        .page_lsn = Lsn{0x0E0D0C0B0A090807ULL},
        .checksum_crc32c = std::uint32_t{0x1211100FU},
        .header_size = std::uint16_t{0x1413U},
        .reserved16 = std::uint16_t{0x1615U},
        .page_no = PageNo{0x1E1D1C1B1A191817ULL},
    };
    std::array<std::byte, COMMON_PAGE_HEADER_ENCODED_SIZE> buffer{};
    constexpr std::array expected{
        std::byte{0x04}, std::byte{0x00}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03},
        std::byte{0x04}, std::byte{0x05}, std::byte{0x06}, std::byte{0x07}, std::byte{0x08},
        std::byte{0x09}, std::byte{0x0A}, std::byte{0x0B}, std::byte{0x0C}, std::byte{0x0D},
        std::byte{0x0E}, std::byte{0x0F}, std::byte{0x10}, std::byte{0x11}, std::byte{0x12},
        std::byte{0x13}, std::byte{0x14}, std::byte{0x15}, std::byte{0x16}, std::byte{0x17},
        std::byte{0x18}, std::byte{0x19}, std::byte{0x1A}, std::byte{0x1B}, std::byte{0x1C},
        std::byte{0x1D}, std::byte{0x1E},
    };

    ASSERT_TRUE(EncodeCommonPageHeader(buffer, header));
    EXPECT_EQ(buffer, expected);
}

TEST(CommonPageHeaderCodecTest, EncodesAndRoundTripsEveryInitiallyLockedPageType) {
    struct PageTypeEncoding {
        PageType page_type;
        std::uint16_t encoded_value;
    };
    constexpr std::array page_type_encodings{
        PageTypeEncoding{.page_type = PageType::SUPERBLOCK, .encoded_value = 0},
        PageTypeEncoding{.page_type = PageType::HEAP_DATA, .encoded_value = 1},
        PageTypeEncoding{.page_type = PageType::FSM_DATA, .encoded_value = 2},
        PageTypeEncoding{.page_type = PageType::BTREE_INTERNAL, .encoded_value = 3},
        PageTypeEncoding{.page_type = PageType::BTREE_LEAF, .encoded_value = 4},
        PageTypeEncoding{.page_type = PageType::BTREE_FREE, .encoded_value = 5},
        PageTypeEncoding{.page_type = PageType::CATALOG_DATA, .encoded_value = 6},
    };

    for (const auto& page_type_encoding : page_type_encodings) {
        std::array<std::byte, COMMON_PAGE_HEADER_ENCODED_SIZE> buffer{};
        const CommonPageHeader expected{.page_type = page_type_encoding.page_type};

        ASSERT_TRUE(EncodeCommonPageHeader(buffer, expected));
        EXPECT_EQ(buffer[0], static_cast<std::byte>(page_type_encoding.encoded_value));
        EXPECT_EQ(buffer[1], std::byte{0});

        const auto decoded = DecodeCommonPageHeader(buffer);
        if (!decoded.has_value()) {
            ADD_FAILURE() << "common page header decode unexpectedly failed";
            continue;
        }
        EXPECT_EQ(*decoded, expected);
    }
}

TEST(CommonPageHeaderCodecTest, RoundTripsMinimumAndMaximumFields) {
    ExpectHeaderRoundTrip(CommonPageHeader{
        .page_type = PageType::SUPERBLOCK,
        .format_version = std::uint16_t{0},
        .flags = std::uint32_t{0},
        .page_lsn = Lsn{0},
        .checksum_crc32c = std::uint32_t{0},
        .header_size = std::uint16_t{0},
        .reserved16 = std::uint16_t{0},
        .page_no = PageNo{0},
    });
    ExpectHeaderRoundTrip(CommonPageHeader{
        .page_type = PageType::CATALOG_DATA,
        .format_version = std::numeric_limits<std::uint16_t>::max(),
        .flags = std::numeric_limits<std::uint32_t>::max(),
        .page_lsn = std::numeric_limits<Lsn>::max(),
        .checksum_crc32c = std::numeric_limits<std::uint32_t>::max(),
        .header_size = std::numeric_limits<std::uint16_t>::max(),
        .reserved16 = std::numeric_limits<std::uint16_t>::max(),
        .page_no = std::numeric_limits<PageNo>::max(),
    });
}

TEST(CommonPageHeaderCodecTest, PreservesUnknownFlagBits) {
    const CommonPageHeader expected{
        .page_type = PageType::HEAP_DATA,
        .flags = std::uint32_t{0xA5F00F5AU},
    };

    ExpectHeaderRoundTrip(expected);
}

TEST(CommonPageHeaderCodecTest, DefaultReservedFieldEncodesAsZero) {
    std::array<std::byte, COMMON_PAGE_HEADER_ENCODED_SIZE> buffer{};
    buffer.fill(std::byte{0xA5});

    ASSERT_TRUE(EncodeCommonPageHeader(buffer, CommonPageHeader{}));
    EXPECT_EQ(buffer[22], std::byte{0});
    EXPECT_EQ(buffer[23], std::byte{0});
}

TEST(CommonPageHeaderCodecTest, SupportsUnalignedSpans) {
    constexpr auto padding = std::byte{0xA5};
    const CommonPageHeader expected{
        .page_type = PageType::FSM_DATA,
        .format_version = std::uint16_t{3},
        .flags = std::uint32_t{5},
        .page_lsn = Lsn{7},
        .checksum_crc32c = std::uint32_t{11},
        .header_size = std::uint16_t{13},
        .reserved16 = std::uint16_t{17},
        .page_no = PageNo{19},
    };
    std::array<std::byte, COMMON_PAGE_HEADER_ENCODED_SIZE + 2> buffer{};
    buffer.fill(padding);
    auto encoded = std::span<std::byte>{buffer}.subspan(1, COMMON_PAGE_HEADER_ENCODED_SIZE);

    ASSERT_TRUE(EncodeCommonPageHeader(encoded, expected));
    EXPECT_EQ(buffer.front(), padding);
    EXPECT_EQ(buffer.back(), padding);

    const auto decoded = DecodeCommonPageHeader(encoded);
    if (!decoded.has_value()) {
        ADD_FAILURE() << "common page header decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(*decoded, expected);
}

TEST(CommonPageHeaderCodecTest, RejectsUndersizedBuffersWithoutModification) {
    constexpr auto padding = std::byte{0xA5};
    std::array<std::byte, COMMON_PAGE_HEADER_ENCODED_SIZE> buffer{};
    buffer.fill(padding);
    const auto original = buffer;
    auto undersized = std::span<std::byte>{buffer}.first(COMMON_PAGE_HEADER_ENCODED_SIZE - 1);

    EXPECT_FALSE(EncodeCommonPageHeader(undersized, CommonPageHeader{}));
    EXPECT_EQ(buffer, original);
    EXPECT_FALSE(DecodeCommonPageHeader(undersized).has_value());
}

} // namespace
} // namespace dblusblus
