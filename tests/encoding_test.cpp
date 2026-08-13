#include "common/encoding.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <span>

namespace dblusblus {
namespace {

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

} // namespace
} // namespace dblusblus
