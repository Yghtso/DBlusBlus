#include "buffer_test_support.h"
#include "storage/buffer/buffer_pool.h"
#include "storage/file/page_file.h"
#include "storage/heap/heap_page.h"
#include "storage/heap/heap_test_tuples.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <vector>

namespace dblusblus {
namespace {

using buffer_test::InitializeHeap;
using buffer_test::Must;
using buffer_test::TemporaryDirectory;
using buffer_test::ValidateHeap;

constexpr FileId FILE_ID = 11;

// A valid 200-byte heap test tuple whose VARCHAR payload bytes are a key-specific pattern.
[[nodiscard]] std::vector<std::byte> TupleFor(std::uint32_t key) {
    auto tuple = heap_test::MakeTestTuple(200);
    for (std::size_t index = heap_test::TEST_TUPLE_MIN_SIZE; index < tuple.size(); ++index) {
        tuple[index] =
            static_cast<std::byte>((static_cast<std::size_t>(key) * 31U + index) & 0xFFU);
    }
    return tuple;
}

// Writes many pages through a three-frame pool so nearly every page is evicted dirty at least
// once, shuts down durably, then reopens with a fresh DiskManager and pool and checks every tuple.
TEST(BufferPoolPersistenceTest, PagesWrittenThroughATinyPoolSurviveEvictionAndReopen) {
    TemporaryDirectory directory;
    ASSERT_TRUE(directory.Valid());
    const auto path = directory.File("relation.heap");
    constexpr std::uint32_t tuple_count = 600;
    std::vector<Rid> rids;

    {
        DiskManager disk;
        auto file = PageFile::Create(disk,
                                     path,
                                     FileSuperblock{.file_kind = FileKind::HEAP,
                                                    .file_id = FILE_ID,
                                                    .page_lsn = INVALID_LSN,
                                                    .object_id = 3,
                                                    .creation_epoch = 1});
        ASSERT_TRUE(file);
        BufferPool pool{disk, BufferPoolOptions{.frame_count = 3}};
        ASSERT_TRUE(pool.RegisterFile(FILE_ID, ValidateHeap));

        PageId current{};
        for (std::uint32_t key = 0; key < tuple_count; ++key) {
            const auto tuple = TupleFor(key);
            if (current.file_id != INVALID_FILE_ID) {
                auto guard = Must(pool.FetchWrite(current));
                ASSERT_TRUE(guard.Valid());
                const auto result = heap_test::TestHeapPage(guard.MutablePage()).Insert(tuple);
                if (result.rid.has_value()) {
                    guard.MarkDirty();
                    rids.push_back(result.rid.value_or(Rid{}));
                    continue;
                }
            }
            auto created = Must(pool.NewPage(FILE_ID, InitializeHeap));
            ASSERT_TRUE(created.Valid());
            const auto result = heap_test::TestHeapPage(created.MutablePage()).Insert(tuple);
            ASSERT_TRUE(result.rid.has_value());
            created.MarkDirty();
            rids.push_back(result.rid.value_or(Rid{}));
            current = created.Id();
        }
        ASSERT_GT(current.page_no, 10U);
        ASSERT_TRUE(pool.Shutdown());
        ASSERT_TRUE(pool.RetireFile(FILE_ID, RetireDisposition::FLUSH_DIRTY));
    }

    DiskManager disk;
    auto file = PageFile::Open(disk, path, FILE_ID, FileKind::HEAP, 3);
    ASSERT_TRUE(file);
    BufferPool pool{disk, BufferPoolOptions{.frame_count = 3}};
    ASSERT_TRUE(pool.RegisterFile(FILE_ID, ValidateHeap));
    for (std::uint32_t key = 0; key < tuple_count; ++key) {
        const Rid rid = rids[key];
        auto guard = Must(pool.FetchRead(rid.page));
        ASSERT_TRUE(guard.Valid()) << "page " << rid.page.page_no;
        const auto bytes = heap_test::TestHeapPage(const_cast<Page&>(guard.GetPage()))
                               .TupleBytes(rid.slot)
                               .value_or(std::span<const std::byte>{});
        const auto expected = TupleFor(key);
        ASSERT_TRUE(std::ranges::equal(bytes, expected)) << "key " << key;
    }
}

} // namespace
} // namespace dblusblus
