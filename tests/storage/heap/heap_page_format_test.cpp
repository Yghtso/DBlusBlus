#include "common/encoding.h"
#include "storage/heap/heap_page_format.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <type_traits>

namespace dblusblus {
namespace {

static_assert(HEAP_PAGE_HEADER_OFFSET == 32);
static_assert(HEAP_PAGE_HEADER_ENCODED_SIZE == 16);
static_assert(HEAP_PAGE_TOTAL_HEADER_SIZE == 48);
static_assert(HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE == 8);
static_assert(HEAP_PAGE_SLOT_DIRECTORY_OFFSET == 48);
static_assert(HEAP_PAGE_MAX_RAW_TUPLE_SIZE == 8135);
static_assert(std::is_same_v<std::underlying_type_t<HeapSlotState>, std::uint16_t>);

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

} // namespace
} // namespace dblusblus
