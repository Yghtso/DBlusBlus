#include "common/encoding.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <span>

namespace dblusblus {
namespace {

static_assert(PAGE_ID_ENCODED_SIZE == 12);
static_assert(RID_ENCODED_SIZE == 16);

template <detail::FixedWidthInteger Integer>
void ExpectRoundTrip(Integer value) {
    std::array<std::byte, sizeof(Integer)> buffer{};

    ASSERT_TRUE(EncodeLittleEndian(buffer, value));
    const auto decoded = DecodeLittleEndian<Integer>(buffer);
    if (!decoded.has_value()) {
        ADD_FAILURE() << "decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(*decoded, value);
}

void ExpectPageIdRoundTrip(const PageId& expected) {
    std::array<std::byte, PAGE_ID_ENCODED_SIZE> buffer{};

    ASSERT_TRUE(EncodePageId(buffer, expected));
    const auto decoded = DecodePageId(buffer);
    if (!decoded.has_value()) {
        ADD_FAILURE() << "PageId decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(*decoded, expected);
}

void ExpectRidRoundTrip(const Rid& expected) {
    std::array<std::byte, RID_ENCODED_SIZE> buffer{};

    ASSERT_TRUE(EncodeRid(buffer, expected));
    const auto decoded = DecodeRid(buffer);
    if (!decoded.has_value()) {
        ADD_FAILURE() << "Rid decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(*decoded, expected);
}

TEST(EncodingTest, EmitsExactLittleEndianBytes) {
    std::array<std::byte, 8> buffer{};
    constexpr std::array expected{
        std::byte{0x08},
        std::byte{0x07},
        std::byte{0x06},
        std::byte{0x05},
        std::byte{0x04},
        std::byte{0x03},
        std::byte{0x02},
        std::byte{0x01},
    };

    ASSERT_TRUE(EncodeLittleEndian(buffer, std::uint64_t{0x0102030405060708ULL}));
    EXPECT_EQ(buffer, expected);
}

TEST(EncodingTest, EncodesSignedValuesUsingTwosComplementBytes) {
    std::array<std::byte, 4> buffer{};
    constexpr std::array expected{
        std::byte{0xFE},
        std::byte{0xFF},
        std::byte{0xFF},
        std::byte{0xFF},
    };

    ASSERT_TRUE(EncodeLittleEndian(buffer, std::int32_t{-2}));
    EXPECT_EQ(buffer, expected);
}

TEST(EncodingTest, DecodesExactLittleEndianBytes) {
    constexpr std::array bytes{
        std::byte{0x78},
        std::byte{0x56},
        std::byte{0x34},
        std::byte{0x12},
    };

    const auto decoded = DecodeLittleEndian<std::uint32_t>(bytes);
    if (!decoded.has_value()) {
        ADD_FAILURE() << "decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(*decoded, std::uint32_t{0x12345678U});
}

TEST(EncodingTest, RoundTripsUnsignedBoundaries) {
    ExpectRoundTrip(std::uint8_t{0});
    ExpectRoundTrip(std::numeric_limits<std::uint8_t>::max());
    ExpectRoundTrip(std::uint16_t{0});
    ExpectRoundTrip(std::numeric_limits<std::uint16_t>::max());
    ExpectRoundTrip(std::uint32_t{0});
    ExpectRoundTrip(std::numeric_limits<std::uint32_t>::max());
    ExpectRoundTrip(std::uint64_t{0});
    ExpectRoundTrip(std::numeric_limits<std::uint64_t>::max());
}

TEST(EncodingTest, RoundTripsSignedBoundaries) {
    ExpectRoundTrip(std::numeric_limits<std::int8_t>::min());
    ExpectRoundTrip(std::numeric_limits<std::int8_t>::max());
    ExpectRoundTrip(std::numeric_limits<std::int16_t>::min());
    ExpectRoundTrip(std::numeric_limits<std::int16_t>::max());
    ExpectRoundTrip(std::numeric_limits<std::int32_t>::min());
    ExpectRoundTrip(std::numeric_limits<std::int32_t>::max());
    ExpectRoundTrip(std::numeric_limits<std::int64_t>::min());
    ExpectRoundTrip(std::numeric_limits<std::int64_t>::max());
}

TEST(EncodingTest, SupportsUnalignedBufferOffsets) {
    constexpr auto padding = std::byte{0xA5};
    std::array<std::byte, 10> buffer{};
    buffer.fill(padding);
    auto field = std::span<std::byte>{buffer}.subspan(1, sizeof(std::uint64_t));

    ASSERT_TRUE(EncodeLittleEndian(field, std::uint64_t{0x0102030405060708ULL}));
    EXPECT_EQ(buffer.front(), padding);
    EXPECT_EQ(buffer.back(), padding);

    const auto decoded = DecodeLittleEndian<std::uint64_t>(field);
    if (!decoded.has_value()) {
        ADD_FAILURE() << "decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(*decoded, std::uint64_t{0x0102030405060708ULL});
}

TEST(EncodingTest, RejectsBuffersThatAreTooSmall) {
    constexpr auto padding = std::byte{0xA5};
    std::array<std::byte, 4> buffer{};
    buffer.fill(padding);
    const auto original = buffer;
    auto short_buffer = std::span<std::byte>{buffer}.first(3);

    EXPECT_FALSE(EncodeLittleEndian(short_buffer, std::uint32_t{42}));
    EXPECT_EQ(buffer, original);
    EXPECT_FALSE(DecodeLittleEndian<std::uint32_t>(short_buffer).has_value());
}

TEST(PageIdCodecTest, UsesLockedEncodedSize) {
    EXPECT_EQ(PAGE_ID_ENCODED_SIZE, std::size_t{12});
}

TEST(PageIdCodecTest, EmitsExactPersistedBytes) {
    const PageId page_id{
        .file_id = FileId{0x01020304U},
        .page_no = PageNo{0x05060708090A0B0CULL},
    };
    std::array<std::byte, PAGE_ID_ENCODED_SIZE> buffer{};
    constexpr std::array expected{
        std::byte{0x04},
        std::byte{0x03},
        std::byte{0x02},
        std::byte{0x01},
        std::byte{0x0C},
        std::byte{0x0B},
        std::byte{0x0A},
        std::byte{0x09},
        std::byte{0x08},
        std::byte{0x07},
        std::byte{0x06},
        std::byte{0x05},
    };

    ASSERT_TRUE(EncodePageId(buffer, page_id));
    EXPECT_EQ(buffer, expected);
}

TEST(PageIdCodecTest, RoundTripsRegularSentinelAndBoundaryValues) {
    ExpectPageIdRoundTrip(PageId{.file_id = FileId{7}, .page_no = PageNo{42}});
    ExpectPageIdRoundTrip(PageId{});
    ExpectPageIdRoundTrip(PageId{.file_id = FileId{0}, .page_no = PageNo{0}});
    ExpectPageIdRoundTrip(PageId{
        .file_id = std::numeric_limits<FileId>::max(),
        .page_no = std::numeric_limits<PageNo>::max(),
    });
}

TEST(PageIdCodecTest, SupportsUnalignedSpans) {
    constexpr auto padding = std::byte{0xA5};
    const PageId expected{.file_id = FileId{11}, .page_no = PageNo{29}};
    std::array<std::byte, PAGE_ID_ENCODED_SIZE + 2> buffer{};
    buffer.fill(padding);
    auto encoded = std::span<std::byte>{buffer}.subspan(1, PAGE_ID_ENCODED_SIZE);

    ASSERT_TRUE(EncodePageId(encoded, expected));
    EXPECT_EQ(buffer.front(), padding);
    EXPECT_EQ(buffer.back(), padding);

    const auto decoded = DecodePageId(encoded);
    if (!decoded.has_value()) {
        ADD_FAILURE() << "PageId decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(*decoded, expected);
}

TEST(PageIdCodecTest, RejectsUndersizedBuffersWithoutModification) {
    constexpr auto padding = std::byte{0xA5};
    std::array<std::byte, PAGE_ID_ENCODED_SIZE> buffer{};
    buffer.fill(padding);
    const auto original = buffer;
    auto undersized = std::span<std::byte>{buffer}.first(PAGE_ID_ENCODED_SIZE - 1);

    EXPECT_FALSE(EncodePageId(undersized, PageId{}));
    EXPECT_EQ(buffer, original);
    EXPECT_FALSE(DecodePageId(undersized).has_value());
}

TEST(RidCodecTest, UsesLockedEncodedSize) {
    EXPECT_EQ(RID_ENCODED_SIZE, std::size_t{16});
}

TEST(RidCodecTest, EmitsExactPersistedBytes) {
    const Rid rid{
        .page =
            PageId{
                .file_id = FileId{0x01020304U},
                .page_no = PageNo{0x05060708090A0B0CULL},
            },
        .slot = SlotId{0x0D0EU},
    };
    std::array<std::byte, RID_ENCODED_SIZE> buffer{};
    constexpr std::array expected{
        std::byte{0x04},
        std::byte{0x03},
        std::byte{0x02},
        std::byte{0x01},
        std::byte{0x0C},
        std::byte{0x0B},
        std::byte{0x0A},
        std::byte{0x09},
        std::byte{0x08},
        std::byte{0x07},
        std::byte{0x06},
        std::byte{0x05},
        std::byte{0x0E},
        std::byte{0x0D},
        std::byte{0x00},
        std::byte{0x00},
    };

    ASSERT_TRUE(EncodeRid(buffer, rid));
    EXPECT_EQ(buffer, expected);
    EXPECT_EQ(buffer[detail::RID_RESERVED_OFFSET], std::byte{0});
    EXPECT_EQ(buffer[detail::RID_RESERVED_OFFSET + 1], std::byte{0});
}

TEST(RidCodecTest, RoundTripsRegularSentinelAndBoundaryValues) {
    ExpectRidRoundTrip(Rid{
        .page = PageId{.file_id = FileId{7}, .page_no = PageNo{42}},
        .slot = SlotId{3},
    });
    ExpectRidRoundTrip(Rid{});
    ExpectRidRoundTrip(Rid{
        .page = PageId{.file_id = FileId{0}, .page_no = PageNo{0}},
        .slot = SlotId{0},
    });
    ExpectRidRoundTrip(Rid{
        .page =
            PageId{
                .file_id = std::numeric_limits<FileId>::max(),
                .page_no = std::numeric_limits<PageNo>::max(),
            },
        .slot = std::numeric_limits<SlotId>::max(),
    });
}

TEST(RidCodecTest, SupportsUnalignedSpans) {
    constexpr auto padding = std::byte{0xA5};
    const Rid expected{
        .page = PageId{.file_id = FileId{11}, .page_no = PageNo{29}},
        .slot = SlotId{5},
    };
    std::array<std::byte, RID_ENCODED_SIZE + 2> buffer{};
    buffer.fill(padding);
    auto encoded = std::span<std::byte>{buffer}.subspan(1, RID_ENCODED_SIZE);

    ASSERT_TRUE(EncodeRid(encoded, expected));
    EXPECT_EQ(buffer.front(), padding);
    EXPECT_EQ(buffer.back(), padding);

    const auto decoded = DecodeRid(encoded);
    if (!decoded.has_value()) {
        ADD_FAILURE() << "Rid decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(*decoded, expected);
}

TEST(RidCodecTest, RejectsUndersizedBuffersWithoutModification) {
    constexpr auto padding = std::byte{0xA5};
    std::array<std::byte, RID_ENCODED_SIZE> buffer{};
    buffer.fill(padding);
    const auto original = buffer;
    auto undersized = std::span<std::byte>{buffer}.first(RID_ENCODED_SIZE - 1);

    EXPECT_FALSE(EncodeRid(undersized, Rid{}));
    EXPECT_EQ(buffer, original);
    EXPECT_FALSE(DecodeRid(undersized).has_value());
}

TEST(RidCodecTest, RejectsNonzeroFirstReservedByte) {
    constexpr std::array invalid_values{std::byte{0x01}, std::byte{0xFF}};

    for (const auto invalid_value : invalid_values) {
        std::array<std::byte, RID_ENCODED_SIZE> buffer{};
        ASSERT_TRUE(EncodeRid(buffer, Rid{}));
        buffer[detail::RID_RESERVED_OFFSET] = invalid_value;

        EXPECT_FALSE(DecodeRid(buffer).has_value());
    }
}

TEST(RidCodecTest, RejectsNonzeroSecondReservedByte) {
    constexpr std::array invalid_values{std::byte{0x01}, std::byte{0xFF}};

    for (const auto invalid_value : invalid_values) {
        std::array<std::byte, RID_ENCODED_SIZE> buffer{};
        ASSERT_TRUE(EncodeRid(buffer, Rid{}));
        buffer[detail::RID_RESERVED_OFFSET + 1] = invalid_value;

        EXPECT_FALSE(DecodeRid(buffer).has_value());
    }
}

TEST(RidCodecTest, RejectsBothNonzeroReservedBytes) {
    std::array<std::byte, RID_ENCODED_SIZE> buffer{};
    ASSERT_TRUE(EncodeRid(buffer, Rid{}));
    buffer[detail::RID_RESERVED_OFFSET] = std::byte{0x01};
    buffer[detail::RID_RESERVED_OFFSET + 1] = std::byte{0xFF};

    EXPECT_FALSE(DecodeRid(buffer).has_value());
}

} // namespace
} // namespace dblusblus
