#include "common/encoding.h"
#include "common/file_superblock.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <type_traits>

namespace dblusblus {
namespace {

constexpr std::size_t CHECKSUM_OFFSET = 16;
constexpr std::size_t HEADER_SIZE_OFFSET = 20;
constexpr std::size_t PAGE_NO_OFFSET = 24;
constexpr std::size_t MAGIC_OFFSET = 32;
constexpr std::size_t FILE_KIND_OFFSET = 40;
constexpr std::size_t RESERVED16_OFFSET = 42;
constexpr std::size_t PAGE_SIZE_OFFSET = 44;
constexpr std::size_t RESERVED32_OFFSET = 52;

static_assert(PAGE_SIZE == 8192);
static_assert(FILE_SUPERBLOCK_MAGIC.size() == 8);
static_assert(FILE_SUPERBLOCK_HEADER_SIZE == 72);
static_assert(std::is_same_v<std::underlying_type_t<FileKind>, std::uint16_t>);

void RefreshChecksum(std::span<std::byte> page) {
    const auto checksum = ComputeFileSuperblockChecksum(page);
    if (!checksum.has_value()) {
        ADD_FAILURE() << "superblock checksum computation unexpectedly failed";
        return;
    }
    ASSERT_TRUE(
        EncodeLittleEndian(page.subspan(CHECKSUM_OFFSET, sizeof(std::uint32_t)), *checksum));
}

template <detail::FixedWidthInteger Integer>
void ReplaceFieldAndRefreshChecksum(std::span<std::byte> page, std::size_t offset, Integer value) {
    ASSERT_TRUE(EncodeLittleEndian(page.subspan(offset, sizeof(Integer)), value));
    RefreshChecksum(page);
}

void ExpectDecodeError(std::span<const std::byte> page, FileSuperblockDecodeError expected) {
    const auto result = DecodeFileSuperblock(page);
    EXPECT_FALSE(result.superblock.has_value());
    EXPECT_EQ(result.error, expected);
}

void ExpectRoundTrip(const FileSuperblock& expected) {
    std::array<std::byte, PAGE_SIZE> page{};
    ASSERT_TRUE(EncodeFileSuperblock(page, expected));

    const auto result = DecodeFileSuperblock(page);
    if (!result.superblock.has_value()) {
        ADD_FAILURE() << "superblock decode unexpectedly failed";
        return;
    }
    EXPECT_EQ(result.error, FileSuperblockDecodeError::NONE);
    EXPECT_EQ(*result.superblock, expected);
}

TEST(FileSuperblockTest, UsesExplicitFormatConstantsAndFileKindCodes) {
    EXPECT_EQ(PAGE_SIZE, std::size_t{8192});
    EXPECT_EQ(FILE_SUPERBLOCK_FORMAT_VERSION, std::uint16_t{1});
    EXPECT_EQ(FILE_SUPERBLOCK_HEADER_SIZE, std::uint16_t{72});
    EXPECT_EQ(static_cast<std::uint16_t>(FileKind::HEAP), std::uint16_t{1});
    EXPECT_EQ(static_cast<std::uint16_t>(FileKind::BTREE), std::uint16_t{2});
    EXPECT_EQ(static_cast<std::uint16_t>(FileKind::FSM), std::uint16_t{3});
    EXPECT_EQ(static_cast<std::uint16_t>(FileKind::CATALOG), std::uint16_t{4});
}

TEST(FileSuperblockTest, EmitsExactPersistedLayoutAndDeterministicReservedBytes) {
    const FileSuperblock superblock{
        .file_kind = FileKind::BTREE,
        .file_id = FileId{0x0C0B0A09U},
        .flags = std::uint32_t{0x04030201U},
        .page_lsn = Lsn{0x0C0B0A0908070605ULL},
        .object_id = std::uint64_t{0x14131211100F0E0DULL},
        .creation_epoch = std::uint64_t{0x1C1B1A1918171615ULL},
    };
    std::array<std::byte, PAGE_SIZE> page{};
    page.fill(std::byte{0xA5});
    constexpr std::array expected_before_checksum{
        std::byte{0x00},
        std::byte{0x00},
        std::byte{0x01},
        std::byte{0x00},
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
    };
    constexpr std::array expected_after_checksum{
        std::byte{0x48}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{'D'},  std::byte{'B'},  std::byte{'L'},
        std::byte{'U'},  std::byte{'S'},  std::byte{'B'},  std::byte{'L'},  std::byte{'S'},
        std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x20}, std::byte{0x00}, std::byte{0x00}, std::byte{0x09}, std::byte{0x0A},
        std::byte{0x0B}, std::byte{0x0C}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x0D}, std::byte{0x0E}, std::byte{0x0F}, std::byte{0x10},
        std::byte{0x11}, std::byte{0x12}, std::byte{0x13}, std::byte{0x14}, std::byte{0x15},
        std::byte{0x16}, std::byte{0x17}, std::byte{0x18}, std::byte{0x19}, std::byte{0x1A},
        std::byte{0x1B}, std::byte{0x1C},
    };

    ASSERT_TRUE(EncodeFileSuperblock(page, superblock));
    EXPECT_TRUE(
        std::equal(expected_before_checksum.begin(), expected_before_checksum.end(), page.begin()));
    EXPECT_TRUE(std::equal(expected_after_checksum.begin(),
                           expected_after_checksum.end(),
                           page.begin() + HEADER_SIZE_OFFSET));
    EXPECT_TRUE(std::all_of(page.begin() + FILE_SUPERBLOCK_HEADER_SIZE,
                            page.end(),
                            [](std::byte byte) { return byte == std::byte{0}; }));

    const auto stored_checksum = DecodeLittleEndian<std::uint32_t>(
        std::span{page}.subspan(CHECKSUM_OFFSET, sizeof(std::uint32_t)));
    const auto computed_checksum = ComputeFileSuperblockChecksum(page);
    if (!stored_checksum.has_value() || !computed_checksum.has_value()) {
        ADD_FAILURE() << "superblock checksum unexpectedly missing";
        return;
    }
    EXPECT_EQ(*stored_checksum, std::uint32_t{0xFCB8C685U});
    EXPECT_EQ(*stored_checksum, *computed_checksum);
}

TEST(FileSuperblockTest, RoundTripsEveryFileKindAndBoundaryValues) {
    constexpr std::array file_kinds{
        FileKind::HEAP,
        FileKind::BTREE,
        FileKind::FSM,
        FileKind::CATALOG,
    };

    for (const auto file_kind : file_kinds) {
        ExpectRoundTrip(FileSuperblock{
            .file_kind = file_kind,
            .file_id = std::numeric_limits<FileId>::max(),
            .flags = std::numeric_limits<std::uint32_t>::max(),
            .page_lsn = std::numeric_limits<Lsn>::max(),
            .object_id = std::numeric_limits<std::uint64_t>::max(),
            .creation_epoch = std::numeric_limits<std::uint64_t>::max(),
        });
    }
}

TEST(FileSuperblockTest, RejectsUndersizedBuffersWithoutModifyingDestination) {
    constexpr auto padding = std::byte{0xA5};
    std::array<std::byte, PAGE_SIZE> page{};
    page.fill(padding);
    const auto original = page;
    auto undersized = std::span<std::byte>{page}.first(PAGE_SIZE - 1);

    EXPECT_FALSE(EncodeFileSuperblock(undersized, FileSuperblock{}));
    EXPECT_EQ(page, original);
    ExpectDecodeError(undersized, FileSuperblockDecodeError::BUFFER_TOO_SMALL);
    EXPECT_FALSE(ComputeFileSuperblockChecksum(undersized).has_value());
}

TEST(FileSuperblockTest, RejectsUnknownFileKindBeforeEncoding) {
    std::array<std::byte, PAGE_SIZE> page{};
    page.fill(std::byte{0xA5});
    const auto original = page;
    const FileSuperblock invalid{
        .file_kind = static_cast<FileKind>(0),
    };

    EXPECT_FALSE(EncodeFileSuperblock(page, invalid));
    EXPECT_EQ(page, original);
}

TEST(FileSuperblockTest, RejectsInvalidIdentityAndFormatFields) {
    std::array<std::byte, PAGE_SIZE> original{};
    ASSERT_TRUE(EncodeFileSuperblock(original, FileSuperblock{}));

    auto page = original;
    page[MAGIC_OFFSET] ^= std::byte{0x01};
    RefreshChecksum(page);
    ExpectDecodeError(page, FileSuperblockDecodeError::MAGIC_MISMATCH);

    page = original;
    ReplaceFieldAndRefreshChecksum(page, 2, std::uint16_t{2});
    ExpectDecodeError(page, FileSuperblockDecodeError::UNSUPPORTED_FORMAT_VERSION);

    page = original;
    ReplaceFieldAndRefreshChecksum(page, PAGE_SIZE_OFFSET, std::uint32_t{4096});
    ExpectDecodeError(page, FileSuperblockDecodeError::WRONG_PAGE_SIZE);

    page = original;
    ReplaceFieldAndRefreshChecksum(page, FILE_KIND_OFFSET, std::uint16_t{0xFFFFU});
    ExpectDecodeError(page, FileSuperblockDecodeError::INVALID_FILE_KIND);
}

TEST(FileSuperblockTest, RejectsInvalidCommonHeaderInvariants) {
    std::array<std::byte, PAGE_SIZE> original{};
    ASSERT_TRUE(EncodeFileSuperblock(original, FileSuperblock{}));

    auto page = original;
    ReplaceFieldAndRefreshChecksum(page, 0, std::uint16_t{1});
    ExpectDecodeError(page, FileSuperblockDecodeError::WRONG_PAGE_TYPE);

    page = original;
    ReplaceFieldAndRefreshChecksum(page, PAGE_NO_OFFSET, PageNo{1});
    ExpectDecodeError(page, FileSuperblockDecodeError::WRONG_PAGE_NUMBER);

    page = original;
    ReplaceFieldAndRefreshChecksum(page, HEADER_SIZE_OFFSET, std::uint16_t{32});
    ExpectDecodeError(page, FileSuperblockDecodeError::WRONG_HEADER_SIZE);
}

TEST(FileSuperblockTest, RejectsNonzeroReservedBytes) {
    std::array<std::byte, PAGE_SIZE> original{};
    ASSERT_TRUE(EncodeFileSuperblock(original, FileSuperblock{}));

    constexpr std::array reserved_offsets{
        std::size_t{22},
        RESERVED16_OFFSET,
        RESERVED32_OFFSET,
        std::size_t{FILE_SUPERBLOCK_HEADER_SIZE},
        PAGE_SIZE - 1,
    };
    for (const auto offset : reserved_offsets) {
        auto page = original;
        page[offset] = std::byte{0xA5};
        RefreshChecksum(page);
        ExpectDecodeError(page, FileSuperblockDecodeError::NONZERO_RESERVED_BYTES);
    }
}

TEST(FileSuperblockTest, VerifiesChecksumAndTreatsStoredFieldAsZero) {
    std::array<std::byte, PAGE_SIZE> page{};
    ASSERT_TRUE(EncodeFileSuperblock(page, FileSuperblock{}));

    const auto original_logical_checksum = ComputeFileSuperblockChecksum(page);
    if (!original_logical_checksum.has_value()) {
        ADD_FAILURE() << "superblock checksum unexpectedly missing";
        return;
    }

    page[CHECKSUM_OFFSET] ^= std::byte{0xFF};
    const auto changed_field_logical_checksum = ComputeFileSuperblockChecksum(page);
    if (!changed_field_logical_checksum.has_value()) {
        ADD_FAILURE() << "superblock checksum unexpectedly missing";
        return;
    }
    EXPECT_EQ(*changed_field_logical_checksum, *original_logical_checksum);
    ExpectDecodeError(page, FileSuperblockDecodeError::CHECKSUM_MISMATCH);

    ASSERT_TRUE(EncodeFileSuperblock(page, FileSuperblock{}));
    page.back() ^= std::byte{0x01};
    ExpectDecodeError(page, FileSuperblockDecodeError::CHECKSUM_MISMATCH);
}

} // namespace
} // namespace dblusblus
