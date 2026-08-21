#include "common/encoding.h"
#include "storage/heap/heap_page.h"
#include "storage/page/page.h"
#include "storage/tuple/tuple_header.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <span>

namespace dblusblus {
namespace {

static_assert(TUPLE_HEADER_XMIN_OFFSET == 0);
static_assert(TUPLE_HEADER_XMAX_OFFSET == 8);
static_assert(TUPLE_HEADER_CMIN_OFFSET == 16);
static_assert(TUPLE_HEADER_CMAX_OFFSET == 20);
static_assert(TUPLE_HEADER_PREV_PAGE_NO_OFFSET == 24);
static_assert(TUPLE_HEADER_PREV_SLOT_OFFSET == 32);
static_assert(TUPLE_HEADER_FLAGS_OFFSET == 34);
static_assert(TUPLE_HEADER_HEADER_BYTES_OFFSET == 36);
static_assert(TUPLE_HEADER_NULL_BITMAP_BYTES_OFFSET == 38);
static_assert(TUPLE_HEADER_SCHEMA_VERSION_OFFSET == 40);
static_assert(TUPLE_HEADER_RESERVED_OFFSET == 44);
static_assert(TUPLE_HEADER_ENCODED_SIZE == 48);
static_assert(TUPLE_FLAG_HAS_NULLS == 0x0001U);
static_assert(TUPLE_FLAG_HAS_VARLEN == 0x0002U);
static_assert(TUPLE_FLAGS_KNOWN_MASK == 0x0003U);

void ExpectRoundTrip(const TupleHeader& expected) {
    std::array<std::byte, TUPLE_HEADER_ENCODED_SIZE> encoded{};
    ASSERT_TRUE(EncodeTupleHeader(encoded, expected));

    const auto decoded = DecodeTupleHeader(encoded);
    if (!decoded.header.has_value()) {
        ADD_FAILURE() << "tuple header unexpectedly failed to decode";
        return;
    }
    EXPECT_EQ(decoded.error, TupleHeaderDecodeError::NONE);
    EXPECT_EQ(*decoded.header, expected);
}

[[nodiscard]] std::array<std::byte, TUPLE_HEADER_ENCODED_SIZE>
EncodeValidHeader(const TupleHeader& header = {}) {
    std::array<std::byte, TUPLE_HEADER_ENCODED_SIZE> encoded{};
    if (!EncodeTupleHeader(encoded, header)) {
        ADD_FAILURE() << "valid tuple header unexpectedly failed to encode";
    }
    return encoded;
}

TEST(TupleHeaderCodecTest, EmitsExactFortyEightByteLittleEndianLayout) {
    const TupleHeader header{
        .xmin = TxnId{0x0807060504030201ULL},
        .xmax = TxnId{0x100F0E0D0C0B0A09ULL},
        .cmin = CommandId{0x14131211U},
        .cmax = CommandId{0x18171615U},
        .prev_page_no = PageNo{0x201F1E1D1C1B1A19ULL},
        .prev_slot = SlotId{0x2221U},
        .tuple_flags = TUPLE_FLAG_HAS_NULLS | TUPLE_FLAG_HAS_VARLEN,
        .header_bytes = TUPLE_HEADER_ENCODED_SIZE,
        .null_bitmap_bytes = 0x2827U,
        .schema_version = SchemaVer{0x2C2B2A29U},
        .reserved = 0,
    };
    constexpr std::array expected{
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}, std::byte{0x05},
        std::byte{0x06}, std::byte{0x07}, std::byte{0x08}, std::byte{0x09}, std::byte{0x0A},
        std::byte{0x0B}, std::byte{0x0C}, std::byte{0x0D}, std::byte{0x0E}, std::byte{0x0F},
        std::byte{0x10}, std::byte{0x11}, std::byte{0x12}, std::byte{0x13}, std::byte{0x14},
        std::byte{0x15}, std::byte{0x16}, std::byte{0x17}, std::byte{0x18}, std::byte{0x19},
        std::byte{0x1A}, std::byte{0x1B}, std::byte{0x1C}, std::byte{0x1D}, std::byte{0x1E},
        std::byte{0x1F}, std::byte{0x20}, std::byte{0x21}, std::byte{0x22}, std::byte{0x03},
        std::byte{0x00}, std::byte{0x30}, std::byte{0x00}, std::byte{0x27}, std::byte{0x28},
        std::byte{0x29}, std::byte{0x2A}, std::byte{0x2B}, std::byte{0x2C}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    };

    const auto encoded = EncodeValidHeader(header);
    EXPECT_EQ(encoded, expected);
}

TEST(TupleHeaderCodecTest, RoundTripsBoundaryAndSentinelValues) {
    const TupleHeader minimum{
        .xmin = INVALID_TXN_ID,
        .xmax = INVALID_TXN_ID,
        .cmin = CommandId{0},
        .cmax = CommandId{0},
        .prev_page_no = PageNo{0},
        .prev_slot = SlotId{0},
        .tuple_flags = 0,
        .header_bytes = TUPLE_HEADER_ENCODED_SIZE,
        .null_bitmap_bytes = 0,
        .schema_version = SchemaVer{0},
        .reserved = 0,
    };
    const TupleHeader maximum{
        .xmin = std::numeric_limits<TxnId>::max(),
        .xmax = std::numeric_limits<TxnId>::max(),
        .cmin = std::numeric_limits<CommandId>::max(),
        .cmax = std::numeric_limits<CommandId>::max(),
        .prev_page_no = INVALID_PAGE_NO,
        .prev_slot = INVALID_SLOT_ID,
        .tuple_flags = TUPLE_FLAGS_KNOWN_MASK,
        .header_bytes = TUPLE_HEADER_ENCODED_SIZE,
        .null_bitmap_bytes = std::numeric_limits<std::uint16_t>::max(),
        .schema_version = std::numeric_limits<SchemaVer>::max(),
        .reserved = 0,
    };

    ExpectRoundTrip(minimum);
    ExpectRoundTrip(maximum);
    ExpectRoundTrip(TupleHeader{});
}

TEST(TupleHeaderCodecTest, RoundTripsEachDefinedFlagAndValidCombinations) {
    for (const TupleFlags flags :
         {TupleFlags{0}, TUPLE_FLAG_HAS_NULLS, TUPLE_FLAG_HAS_VARLEN, TUPLE_FLAGS_KNOWN_MASK}) {
        TupleHeader header{};
        header.tuple_flags = flags;
        ExpectRoundTrip(header);
    }
}

TEST(TupleHeaderCodecTest, RejectsUnknownFlagBitsOnEncodeAndDecode) {
    constexpr TupleFlags unknown_flag = 0x0004U;
    TupleHeader header{};
    header.tuple_flags = unknown_flag;

    std::array<std::byte, TUPLE_HEADER_ENCODED_SIZE> destination{};
    destination.fill(std::byte{0xA5});
    const auto original = destination;
    EXPECT_FALSE(EncodeTupleHeader(destination, header));
    EXPECT_EQ(destination, original);

    auto encoded = EncodeValidHeader();
    ASSERT_TRUE(EncodeLittleEndian(
        std::span<std::byte>{encoded}.subspan(TUPLE_HEADER_FLAGS_OFFSET, sizeof(TupleFlags)),
        unknown_flag));
    const auto decoded = DecodeTupleHeader(encoded);
    EXPECT_FALSE(decoded.header.has_value());
    EXPECT_EQ(decoded.error, TupleHeaderDecodeError::INVALID_FLAGS);
}

TEST(TupleHeaderCodecTest, RequiresConsistentPreviousVersionSentinelPair) {
    for (const TupleHeader header : {
             TupleHeader{.prev_page_no = INVALID_PAGE_NO, .prev_slot = SlotId{3}},
             TupleHeader{.prev_page_no = PageNo{7}, .prev_slot = INVALID_SLOT_ID},
         }) {
        std::array<std::byte, TUPLE_HEADER_ENCODED_SIZE> destination{};
        destination.fill(std::byte{0xA5});
        const auto original = destination;
        EXPECT_FALSE(EncodeTupleHeader(destination, header));
        EXPECT_EQ(destination, original);
    }

    auto invalid_page =
        EncodeValidHeader(TupleHeader{.prev_page_no = PageNo{7}, .prev_slot = SlotId{3}});
    ASSERT_TRUE(EncodeLittleEndian(std::span<std::byte>{invalid_page}.subspan(
                                       TUPLE_HEADER_PREV_PAGE_NO_OFFSET, sizeof(PageNo)),
                                   INVALID_PAGE_NO));
    EXPECT_EQ(DecodeTupleHeader(invalid_page).error,
              TupleHeaderDecodeError::INVALID_PREVIOUS_VERSION_POINTER);

    auto invalid_slot =
        EncodeValidHeader(TupleHeader{.prev_page_no = PageNo{7}, .prev_slot = SlotId{3}});
    ASSERT_TRUE(EncodeLittleEndian(
        std::span<std::byte>{invalid_slot}.subspan(TUPLE_HEADER_PREV_SLOT_OFFSET, sizeof(SlotId)),
        INVALID_SLOT_ID));
    EXPECT_EQ(DecodeTupleHeader(invalid_slot).error,
              TupleHeaderDecodeError::INVALID_PREVIOUS_VERSION_POINTER);
}

TEST(TupleHeaderCodecTest, RequiresFixedHeaderSizeAndZeroReservedField) {
    TupleHeader invalid_size{};
    invalid_size.header_bytes = TUPLE_HEADER_ENCODED_SIZE - 1;
    TupleHeader nonzero_reserved{};
    nonzero_reserved.reserved = 1;

    std::array<std::byte, TUPLE_HEADER_ENCODED_SIZE> destination{};
    EXPECT_FALSE(EncodeTupleHeader(destination, invalid_size));
    EXPECT_FALSE(EncodeTupleHeader(destination, nonzero_reserved));

    auto encoded_size = EncodeValidHeader();
    ASSERT_TRUE(EncodeLittleEndian(std::span<std::byte>{encoded_size}.subspan(
                                       TUPLE_HEADER_HEADER_BYTES_OFFSET, sizeof(std::uint16_t)),
                                   std::uint16_t{47}));
    EXPECT_EQ(DecodeTupleHeader(encoded_size).error, TupleHeaderDecodeError::INVALID_HEADER_SIZE);

    auto encoded_reserved = EncodeValidHeader();
    ASSERT_TRUE(EncodeLittleEndian(std::span<std::byte>{encoded_reserved}.subspan(
                                       TUPLE_HEADER_RESERVED_OFFSET, sizeof(std::uint32_t)),
                                   std::uint32_t{1}));
    EXPECT_EQ(DecodeTupleHeader(encoded_reserved).error, TupleHeaderDecodeError::NONZERO_RESERVED);
}

TEST(TupleHeaderCodecTest, SupportsUnalignedSpans) {
    constexpr auto padding = std::byte{0xA5};
    const TupleHeader expected{
        .xmin = TxnId{17},
        .xmax = INVALID_TXN_ID,
        .cmin = CommandId{0},
        .cmax = CommandId{23},
        .prev_page_no = PageNo{31},
        .prev_slot = SlotId{5},
        .tuple_flags = TUPLE_FLAG_HAS_VARLEN,
        .header_bytes = TUPLE_HEADER_ENCODED_SIZE,
        .null_bitmap_bytes = 9,
        .schema_version = SchemaVer{4},
        .reserved = 0,
    };
    std::array<std::byte, TUPLE_HEADER_ENCODED_SIZE + 2> buffer{};
    buffer.fill(padding);
    auto unaligned = std::span<std::byte>{buffer}.subspan(1, TUPLE_HEADER_ENCODED_SIZE);

    ASSERT_TRUE(EncodeTupleHeader(unaligned, expected));
    EXPECT_EQ(buffer.front(), padding);
    EXPECT_EQ(buffer.back(), padding);
    const auto decoded = DecodeTupleHeader(unaligned);
    if (!decoded.header.has_value()) {
        ADD_FAILURE() << "unaligned tuple header unexpectedly failed to decode";
        return;
    }
    EXPECT_EQ(*decoded.header, expected);
}

TEST(TupleHeaderCodecTest, RejectsUndersizedBuffersWithoutModification) {
    constexpr auto padding = std::byte{0xA5};
    std::array<std::byte, TUPLE_HEADER_ENCODED_SIZE> buffer{};
    buffer.fill(padding);
    const auto original = buffer;
    auto undersized = std::span<std::byte>{buffer}.first(TUPLE_HEADER_ENCODED_SIZE - 1);

    EXPECT_FALSE(EncodeTupleHeader(undersized, TupleHeader{}));
    EXPECT_EQ(buffer, original);
    const auto decoded = DecodeTupleHeader(undersized);
    EXPECT_FALSE(decoded.header.has_value());
    EXPECT_EQ(decoded.error, TupleHeaderDecodeError::SOURCE_TOO_SMALL);
}

TEST(TupleHeaderCodecTest, ComposesWithOpaqueHeapPageInsertion) {
    const TupleHeader expected_header{
        .xmin = TxnId{41},
        .xmax = INVALID_TXN_ID,
        .cmin = CommandId{0},
        .cmax = CommandId{0},
        .prev_page_no = INVALID_PAGE_NO,
        .prev_slot = INVALID_SLOT_ID,
        .tuple_flags = TUPLE_FLAG_HAS_NULLS | TUPLE_FLAG_HAS_VARLEN,
        .header_bytes = TUPLE_HEADER_ENCODED_SIZE,
        .null_bitmap_bytes = 1,
        .schema_version = SchemaVer{7},
        .reserved = 0,
    };
    constexpr std::array payload{
        std::byte{0x80},
        std::byte{0x00},
        std::byte{0x7F},
        std::byte{0x42},
        std::byte{0x11},
    };
    std::array<std::byte, TUPLE_HEADER_ENCODED_SIZE + payload.size()> tuple{};
    ASSERT_TRUE(EncodeTupleHeader(std::span<std::byte>{tuple}.first(TUPLE_HEADER_ENCODED_SIZE),
                                  expected_header));
    std::ranges::copy(payload, tuple.begin() + TUPLE_HEADER_ENCODED_SIZE);

    const PageId page_id{.file_id = FileId{9}, .page_no = PageNo{4}};
    Page page{page_id};
    HeapPage heap_page{page};
    ASSERT_TRUE(heap_page.Initialize());
    const auto insertion = heap_page.Insert(tuple);
    if (!insertion.rid.has_value()) {
        ADD_FAILURE() << "opaque tuple unexpectedly failed to insert";
        return;
    }
    EXPECT_EQ(*insertion.rid, (Rid{.page = page_id, .slot = SlotId{0}}));

    const auto stored = heap_page.TupleBytes(0);
    if (!stored.has_value()) {
        ADD_FAILURE() << "inserted opaque tuple unexpectedly unavailable";
        return;
    }
    const auto stored_bytes = *stored;
    ASSERT_EQ(stored_bytes.size(), tuple.size());
    EXPECT_TRUE(std::ranges::equal(stored_bytes, tuple));

    const auto decoded = DecodeTupleHeader(stored_bytes.first(TUPLE_HEADER_ENCODED_SIZE));
    if (!decoded.header.has_value()) {
        ADD_FAILURE() << "stored tuple header unexpectedly failed to decode";
        return;
    }
    EXPECT_EQ(*decoded.header, expected_header);
    EXPECT_TRUE(std::ranges::equal(stored_bytes.subspan(TUPLE_HEADER_ENCODED_SIZE), payload));
}

} // namespace
} // namespace dblusblus
