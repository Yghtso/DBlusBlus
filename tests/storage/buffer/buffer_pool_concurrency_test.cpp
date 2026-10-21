#include "buffer_test_support.h"
#include "storage/buffer/buffer_pool.h"
#include "storage/buffer/buffer_pool_inspector.h"
#include "storage/heap/heap_page.h"
#include "support/require.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <thread>
#include <vector>

namespace dblusblus::storage {
namespace {

using test::ErrorCodeOf;

using buffer_test::EntryOf;
using buffer_test::FrameOf;
using buffer_test::HeapFileFixture;
using buffer_test::InitializeHeap;
using buffer_test::Must;
using buffer_test::PausingObserver;
using buffer_test::Pid;
using buffer_test::ValidateHeap;
using buffer_test::WaitFor;

// Deterministic interleavings: threads are parked at named BufferPool events and released
// explicitly. Joins wait for observable pool state, never for elapsed time.
class BufferPoolConcurrencyTest : public ::testing::Test {
  protected:
    void SetUp() override {
        ASSERT_TRUE(file_.Create());
    }

    void MakePool(std::size_t frame_count) {
        pool_ = std::make_unique<BufferPool>(
            file_.Io(), BufferPoolOptions{.frame_count = frame_count, .observer = &observer_});
        ASSERT_TRUE(pool_->RegisterFile(file_.TestFileId(), ValidateHeap));
    }

    void AppendRawPages(std::size_t count) {
        for (std::size_t index = 0; index < count; ++index) {
            ASSERT_TRUE(file_.AppendRawHeapPage().has_value());
        }
    }

    [[nodiscard]] std::optional<PageTableSnapshot> Entry(PageId page_id) const {
        return BufferPoolInspector{*pool_}.InspectPage(page_id);
    }

    HeapFileFixture file_;
    PausingObserver observer_;
    std::unique_ptr<BufferPool> pool_;
};

TEST_F(BufferPoolConcurrencyTest, SamePageMissesShareOneLoaderAndOneFrame) {
    AppendRawPages(1);
    MakePool(3);
    observer_.PauseAt(BufferPoolEventType::FRAME_BOUND_LOADING, Pid(1));

    std::array<ReadPageGuard, 3> guards;
    std::array<std::thread, 3> fetchers;
    fetchers[0] = std::thread{[&] { guards[0] = Must(pool_->FetchRead(Pid(1))); }};
    observer_.WaitUntilPaused();

    // The intent is LOADING and not yet usable; joiners register claims without pins.
    const auto loading = EntryOf(*pool_, Pid(1));
    EXPECT_EQ(loading.state, PageTableEntryState::LOADING);
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).state, FrameState::LOADING);
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).io_state, FrameIoState::READ_IN_PROGRESS);
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).pin_count, 0U);

    for (std::size_t index = 1; index < 3; ++index) {
        fetchers[index] =
            std::thread{[&, index] { guards[index] = Must(pool_->FetchRead(Pid(1))); }};
    }
    WaitFor([&] { return EntryOf(*pool_, Pid(1)).pending_claims == 3U; });
    observer_.Resume();
    for (auto& fetcher : fetchers) {
        fetcher.join();
    }

    EXPECT_EQ(file_.Io().ReadCount(), 1U);
    const auto resident = EntryOf(*pool_, Pid(1));
    EXPECT_EQ(resident.state, PageTableEntryState::RESIDENT);
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).pin_count, 3U);
    for (const auto& guard : guards) {
        ASSERT_TRUE(guard.Valid());
        EXPECT_EQ(guard.Frame(), FrameOf(*pool_, Pid(1)).frame_id);
    }
    EXPECT_EQ(pool_->FreeFrameCount(), 2U);
}

TEST_F(BufferPoolConcurrencyTest, LoadFailureReachesEveryJoinerAndIsNotCached) {
    AppendRawPages(1);
    MakePool(3);
    file_.Io().FailNextRead();
    observer_.PauseAt(BufferPoolEventType::FRAME_BOUND_LOADING, Pid(1));

    std::array<std::optional<BufferErrorCode>, 2> codes{};
    std::thread loader{[&] { codes[0] = ErrorCodeOf(pool_->FetchRead(Pid(1))); }};
    observer_.WaitUntilPaused();
    std::thread joiner{[&] { codes[1] = ErrorCodeOf(pool_->FetchRead(Pid(1))); }};
    WaitFor([&] { return EntryOf(*pool_, Pid(1)).pending_claims == 2U; });
    observer_.Resume();
    loader.join();
    joiner.join();

    EXPECT_EQ(codes[0], BufferErrorCode::RAW_IO_FAILURE);
    EXPECT_EQ(codes[1], BufferErrorCode::RAW_IO_FAILURE);
    EXPECT_EQ(file_.Io().ReadCount(), 1U);
    EXPECT_FALSE(Entry(Pid(1)).has_value());
    EXPECT_EQ(pool_->FreeFrameCount(), 3U);
    EXPECT_TRUE(pool_->FetchRead(Pid(1)));
}

TEST_F(BufferPoolConcurrencyTest, FetchThatPinsFirstMakesTheVictimReservationFail) {
    AppendRawPages(2);
    MakePool(1);
    ASSERT_TRUE(pool_->FetchRead(Pid(1))); // resident, unpinned, evictable
    observer_.PauseAt(BufferPoolEventType::VICTIM_CANDIDATE_OBSERVED, Pid(1));

    std::optional<BufferErrorCode> evictor_code;
    std::thread evictor{[&] { evictor_code = ErrorCodeOf(pool_->FetchRead(Pid(2))); }};
    observer_.WaitUntilPaused();

    // The hit wins the race before the final reservation.
    auto hit = Must(pool_->FetchRead(Pid(1)));
    ASSERT_TRUE(hit.Valid());
    observer_.Resume();
    evictor.join();

    EXPECT_EQ(evictor_code, BufferErrorCode::NO_REPLACEABLE_FRAME);
    const auto entry = EntryOf(*pool_, Pid(1));
    EXPECT_EQ(entry.state, PageTableEntryState::RESIDENT);
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).pin_count, 1U);
    EXPECT_FALSE(Entry(Pid(2)).has_value());
}

TEST_F(BufferPoolConcurrencyTest, FetchAfterVictimReservationWaitsAndReloads) {
    AppendRawPages(2);
    MakePool(1);
    {
        auto guard = Must(pool_->FetchWrite(Pid(1)));
        ASSERT_TRUE(guard.Valid());
        const auto tuple =
            heap_test::MakeTestTuple(heap_test::TEST_TUPLE_MIN_SIZE + 1, std::byte{1});
        ASSERT_TRUE(heap_test::TestHeapPage(guard.MutablePage()).Insert(tuple));
        guard.MarkDirty();
    }
    observer_.PauseAt(BufferPoolEventType::WRITEBACK_WRITE_STARTED, Pid(1));

    std::thread evictor{[&] {
        auto guard = pool_->FetchRead(Pid(2));
        EXPECT_TRUE(guard);
    }};
    observer_.WaitUntilPaused();

    // The old mapping exists but cannot be pinned.
    const auto evicting = EntryOf(*pool_, Pid(1));
    EXPECT_EQ(evicting.state, PageTableEntryState::EVICTING);
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).io_state, FrameIoState::WRITEBACK_IN_PROGRESS);
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).pin_count, 0U);

    std::optional<BufferErrorCode> fetched_code;
    std::thread fetcher{[&] {
        auto result = pool_->FetchRead(Pid(1));
        fetched_code = ErrorCodeOf(result);
        if (result) {
            EXPECT_TRUE(
                heap_test::TestHeapPage(const_cast<Page&>(result->GetPage())).TupleBytes(0));
        }
    }};
    observer_.Resume();
    evictor.join();
    fetcher.join();

    // Either the fetcher reloaded A after the eviction, or it found no frame while B was still
    // pinned. It never pinned the reserved victim.
    EXPECT_TRUE(!fetched_code.has_value() || fetched_code == BufferErrorCode::NO_REPLACEABLE_FRAME);
    EXPECT_EQ(file_.Io().WriteCount(), 1U);
}

TEST_F(BufferPoolConcurrencyTest, MutationDuringWritebackKeepsTheNewerGenerationDirty) {
    MakePool(3);
    {
        auto created = Must(pool_->NewPage(file_.TestFileId(), InitializeHeap));
        ASSERT_TRUE(created.Valid());
    }
    // Record the frame's dirty flag when the second writeback copies its image.
    std::atomic<int> copies{0};
    std::atomic<bool> dirty_after_first_writeback{false};
    observer_.SetCallback([&](const BufferPoolEvent& event) {
        if (event.type == BufferPoolEventType::WRITEBACK_IMAGE_COPIED && ++copies == 2) {
            dirty_after_first_writeback =
                BufferPoolInspector{*pool_}.InspectFrame(*event.frame_id).dirty;
        }
    });
    observer_.PauseAt(BufferPoolEventType::WRITEBACK_WRITE_STARTED, Pid(1));

    std::thread flusher{[&] { EXPECT_TRUE(pool_->FlushPage(Pid(1))); }};
    observer_.WaitUntilPaused();

    // Generation G is copied; publish G+1 while its I/O is in flight.
    const auto copied_generation = FrameOf(*pool_, Pid(1)).modification_generation;
    {
        auto guard = Must(pool_->FetchWrite(Pid(1)));
        ASSERT_TRUE(guard.Valid());
        const auto tuple =
            heap_test::MakeTestTuple(heap_test::TEST_TUPLE_MIN_SIZE + 1, std::byte{0x77});
        ASSERT_TRUE(heap_test::TestHeapPage(guard.MutablePage()).Insert(tuple));
        guard.MarkDirty();
    }
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).modification_generation, copied_generation + 1U);
    observer_.Resume();
    flusher.join();

    // G's completion did not clean G+1; the explicit flush repeated and persisted it.
    EXPECT_TRUE(dirty_after_first_writeback.load());
    EXPECT_EQ(file_.Io().WriteCount(), 2U);
    EXPECT_FALSE(FrameOf(*pool_, Pid(1)).dirty);
    auto on_disk = file_.ReadRawPage(Pid(1));
    EXPECT_EQ(buffer_test::FirstTupleByte(on_disk), std::byte{0x77});
}

TEST_F(BufferPoolConcurrencyTest, GuardIsNotEvictableBetweenLatchReleaseAndUnpin) {
    AppendRawPages(2);
    MakePool(1);
    observer_.PauseAt(BufferPoolEventType::GUARD_LATCH_RELEASED, Pid(1));

    std::thread releaser{[&] {
        auto guard = Must(pool_->FetchRead(Pid(1)));
        ASSERT_TRUE(guard.Valid());
        guard.Release();
    }};
    observer_.WaitUntilPaused();
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).pin_count, 1U);
    EXPECT_EQ(pool_->FetchRead(Pid(2)).Error().code, BufferErrorCode::NO_REPLACEABLE_FRAME);
    observer_.Resume();
    releaser.join();
    EXPECT_TRUE(pool_->FetchRead(Pid(2)));
}

TEST_F(BufferPoolConcurrencyTest, ReadersShareTheLatchAndAWriterWaitsHoldingItsPin) {
    AppendRawPages(1);
    MakePool(3);
    auto first = Must(pool_->FetchRead(Pid(1)));
    ASSERT_TRUE(first.Valid());
    std::thread second_reader{[&] { EXPECT_TRUE(pool_->FetchRead(Pid(1))); }};
    second_reader.join(); // completes while `first` holds a shared latch

    std::atomic<bool> writer_acquired{false};
    std::thread writer{[&] {
        auto guard = Must(pool_->FetchWrite(Pid(1)));
        ASSERT_TRUE(guard.Valid());
        writer_acquired = true;
    }};
    // Pin before latch: the waiting writer is visible as a second pin.
    WaitFor([&] { return FrameOf(*pool_, Pid(1)).pin_count == 2U; });
    EXPECT_FALSE(writer_acquired.load());
    first.Release();
    writer.join();
    EXPECT_TRUE(writer_acquired.load());
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).pin_count, 0U);
}

// Randomized multi-threaded workload against a tiny pool. Each page holds one counter tuple that
// is incremented under a write guard; the final counters must equal the increments performed.
TEST(BufferPoolStressTest, ConcurrentIncrementsSurviveEvictionAndReload) {
    HeapFileFixture file;
    ASSERT_TRUE(file.Create());
    constexpr std::size_t page_count = 12;
    constexpr std::size_t thread_count = 4;
    constexpr std::size_t operations_per_thread = 300;
    constexpr std::size_t counter_tuple_size =
        heap_test::TEST_TUPLE_MIN_SIZE + sizeof(std::uint64_t);

    BufferPool pool{file.Io(), BufferPoolOptions{.frame_count = 4}};
    ASSERT_TRUE(pool.RegisterFile(file.TestFileId(), ValidateHeap));
    for (std::size_t index = 0; index < page_count; ++index) {
        auto created = Must(pool.NewPage(file.TestFileId(), InitializeHeap));
        ASSERT_TRUE(created.Valid());
        // The counter is the tuple's 8-byte VARCHAR payload, which ends the tuple.
        const auto zero = heap_test::MakeTestTuple(counter_tuple_size, std::byte{0});
        ASSERT_TRUE(heap_test::TestHeapPage(created.MutablePage()).Insert(zero));
        created.MarkDirty();
    }

    std::array<std::atomic<std::uint64_t>, page_count> expected{};
    std::vector<std::thread> workers;
    for (std::size_t worker = 0; worker < thread_count; ++worker) {
        workers.emplace_back([&, worker] {
            std::mt19937 random{static_cast<std::mt19937::result_type>(worker + 1U)};
            std::uniform_int_distribution<std::size_t> pick{0, page_count - 1U};
            std::uniform_int_distribution<int> action{0, 9};
            for (std::size_t op = 0; op < operations_per_thread; ++op) {
                const PageId page_id = Pid(pick(random) + 1U);
                const int choice = action(random);
                if (choice < 6) {
                    auto guard = pool.FetchWrite(page_id);
                    if (!guard) {
                        // Every frame may be pinned by the other workers at this instant.
                        ASSERT_EQ(guard.Error().code, BufferErrorCode::NO_REPLACEABLE_FRAME);
                        continue;
                    }
                    HeapPage heap{guard->MutablePage(), heap_test::TestSchemaHistory()};
                    auto tuple = heap.TupleBytes(0);
                    ASSERT_TRUE(tuple.has_value());
                    auto bytes = guard->MutableBytes().subspan(
                        static_cast<std::size_t>(tuple->data() - guard->Bytes().data()) +
                            heap_test::TEST_TUPLE_MIN_SIZE,
                        sizeof(std::uint64_t));
                    std::uint64_t value = 0;
                    std::memcpy(&value, bytes.data(), sizeof(value));
                    ++value;
                    std::memcpy(bytes.data(), &value, sizeof(value));
                    guard->MarkDirty();
                    ++expected[page_id.page_no - 1U];
                } else if (choice < 9) {
                    auto guard = pool.FetchRead(page_id);
                    if (!guard) {
                        ASSERT_EQ(guard.Error().code, BufferErrorCode::NO_REPLACEABLE_FRAME);
                    }
                } else {
                    ASSERT_TRUE(pool.FlushPage(page_id));
                }
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    ASSERT_TRUE(pool.Shutdown());

    for (std::size_t index = 0; index < page_count; ++index) {
        auto on_disk = file.ReadRawPage(Pid(index + 1U));
        const auto tuple =
            heap_test::TestHeapPage(on_disk).TupleBytes(0).value_or(std::span<const std::byte>{});
        ASSERT_EQ(tuple.size(), counter_tuple_size);
        std::uint64_t value = 0;
        std::memcpy(&value, tuple.last(sizeof(value)).data(), sizeof(value));
        EXPECT_EQ(value, expected[index].load()) << "page " << index + 1U;
    }
}

} // namespace
} // namespace dblusblus::storage
