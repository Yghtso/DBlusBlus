#ifndef DBLUSBLUS_TESTS_STORAGE_HEAP_HEAP_TEST_TUPLES_H_
#define DBLUSBLUS_TESTS_STORAGE_HEAP_HEAP_TEST_TUPLES_H_

#include "common/types.h"
#include "storage/heap/heap_page.h"
#include "storage/page/page.h"
#include "storage/tuple/tuple_codec.h"
#include "storage/tuple/tuple_layout.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <vector>

namespace dblusblus::heap_test {

// Heap pages validate every retained tuple under a schema, so heap tests store real tuples.
// The test relation has one NOT NULL VARCHAR column: a 48-byte header, a 1-byte null bitmap,
// an 8-byte VARCHAR descriptor, and then the payload. Any size from TEST_TUPLE_MIN_SIZE up to
// the inline limit is reachable by choosing the payload length.
inline constexpr SchemaVer TEST_SCHEMA_VERSION = 1;
inline constexpr std::size_t TEST_TUPLE_MIN_SIZE = 57;

[[nodiscard]] inline std::span<const TuplePhysicalLayout> TestTupleLayouts() {
    static const std::vector<TuplePhysicalLayout> layouts = [] {
        constexpr std::array columns{
            PhysicalColumnSpec{.type = PhysicalType::VARCHAR, .nullable = false},
        };
        auto built = BuildTuplePhysicalLayout(columns, TEST_SCHEMA_VERSION);
        if (!built.layout.has_value()) {
            ADD_FAILURE() << "test tuple layout unexpectedly failed to build";
            return std::vector<TuplePhysicalLayout>{};
        }
        return std::vector<TuplePhysicalLayout>{*built.layout};
    }();
    return layouts;
}

[[nodiscard]] inline HeapTupleSchemaHistory TestSchemaHistory() {
    return HeapTupleSchemaHistory{TestTupleLayouts()};
}

[[nodiscard]] inline HeapPage TestHeapPage(Page& page) {
    return HeapPage{page, TestSchemaHistory()};
}

[[nodiscard]] inline TupleVersionMetadata TestTupleMetadata() {
    return TupleVersionMetadata{.xmin = FIRST_NORMAL_TXN_ID};
}

// Returns a valid encoded tuple of exactly `size` bytes whose payload bytes are all `fill`.
[[nodiscard]] inline std::vector<std::byte>
MakeTestTuple(std::size_t size,
              std::byte fill = std::byte{0x5A},
              const TupleVersionMetadata& metadata = TestTupleMetadata()) {
    if (size < TEST_TUPLE_MIN_SIZE || TestTupleLayouts().empty()) {
        ADD_FAILURE() << "test tuple size " << size << " is below the test schema minimum";
        return {};
    }
    const std::vector<std::byte> payload(size - TEST_TUPLE_MIN_SIZE, fill);
    const std::array values{TupleValue{VarcharValue{.bytes = payload}}};
    auto encoded = EncodeTuple(TestTupleLayouts().front(), metadata, values);
    if (!encoded.tuple.has_value() || encoded.tuple->size() != size) {
        ADD_FAILURE() << "test tuple encoding unexpectedly failed";
        return {};
    }
    return std::move(*encoded.tuple);
}

// Writes a valid test tuple of `size` bytes at `offset` without touching the slot directory.
inline void WriteTestTuple(Page& page,
                           std::size_t offset,
                           std::size_t size,
                           std::byte fill = std::byte{0x5A},
                           const TupleVersionMetadata& metadata = TestTupleMetadata()) {
    const auto tuple = MakeTestTuple(size, fill, metadata);
    ASSERT_EQ(tuple.size(), size);
    ASSERT_LE(offset + size, page.Bytes().size());
    std::ranges::copy(tuple, page.Bytes().begin() + static_cast<std::ptrdiff_t>(offset));
}

} // namespace dblusblus::heap_test

#endif // DBLUSBLUS_TESTS_STORAGE_HEAP_HEAP_TEST_TUPLES_H_
