#include "common/crc32c.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <string_view>

namespace dblusblus {
namespace {

std::span<const std::byte> AsBytes(std::string_view text) {
    return std::as_bytes(std::span{text.data(), text.size()});
}

TEST(Crc32cTest, EmptyInputHasZeroChecksum) {
    EXPECT_EQ(Crc32c({}), std::uint32_t{0x00000000U});
}

TEST(Crc32cTest, MatchesCanonicalTextVectors) {
    EXPECT_EQ(Crc32c(AsBytes("a")), std::uint32_t{0xC1D04330U});
    EXPECT_EQ(Crc32c(AsBytes("abc")), std::uint32_t{0x364B3FB7U});
    EXPECT_EQ(Crc32c(AsBytes("123456789")), std::uint32_t{0xE3069283U});
}

// Binary CRC32C reference vectors from RFC 3720, section B.4.
TEST(Crc32cTest, MatchesThirtyTwoZeroByteReferenceVector) {
    constexpr std::array<std::byte, 32> bytes{};

    EXPECT_EQ(Crc32c(bytes), std::uint32_t{0x8A9136AAU});
}

TEST(Crc32cTest, MatchesThirtyTwoOneByteReferenceVector) {
    std::array<std::byte, 32> bytes{};
    bytes.fill(std::byte{0xFF});

    EXPECT_EQ(Crc32c(bytes), std::uint32_t{0x62A8AB43U});
}

TEST(Crc32cTest, MatchesAscendingAndDescendingBinaryReferenceVectors) {
    std::array<std::byte, 32> ascending{};
    std::array<std::byte, 32> descending{};
    for (std::size_t index = 0; index < ascending.size(); ++index) {
        ascending[index] = static_cast<std::byte>(index);
        descending[index] = static_cast<std::byte>(descending.size() - index - 1U);
    }

    EXPECT_EQ(Crc32c(ascending), std::uint32_t{0x46DD794EU});
    EXPECT_EQ(Crc32c(descending), std::uint32_t{0x113FDB5CU});
}

TEST(Crc32cTest, SupportsUnalignedSubspansAndIsDeterministic) {
    constexpr auto padding = std::byte{0xA5};
    std::array<std::byte, 34> storage{};
    storage.fill(padding);
    auto bytes = std::span<std::byte>{storage}.subspan(1, 32);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::byte>(index);
    }

    const auto first = Crc32c(bytes);
    const auto second = Crc32c(bytes);

    EXPECT_EQ(first, std::uint32_t{0x46DD794EU});
    EXPECT_EQ(second, first);
    EXPECT_EQ(storage.front(), padding);
    EXPECT_EQ(storage.back(), padding);
}

} // namespace
} // namespace dblusblus
