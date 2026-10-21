#include "buffer_test_support.h"
#include "storage/buffer/buffer_pool.h"
#include "storage/buffer/buffer_pool_inspector.h"
#include "storage/heap/heap_page.h"
#include "storage/page/page_checksum.h"
#include "storage/page/page_header.h"
#include "support/require.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace dblusblus::storage {
namespace {

using buffer_test::EntryOf;
using buffer_test::FrameOf;
using buffer_test::HeapFileFixture;
using buffer_test::InitializeHeap;
using buffer_test::Must;
using buffer_test::Pid;
using buffer_test::ValidateHeap;
using buffer_test::WaitFor;
using buffer_test::WriteFault;
using test::ErrorCodeOf;

class BufferPoolTest : public ::testing::Test {
  protected:
    void SetUp() override {
        ASSERT_TRUE(file_.Create());
    }

    void MakePool(std::size_t frame_count, std::uint32_t max_pins = UINT32_MAX) {
        pool_ = std::make_unique<BufferPool>(file_.Io(),
                                             BufferPoolOptions{.frame_count = frame_count,
                                                               .max_pin_count = max_pins,
                                                               .observer = nullptr});
        ASSERT_TRUE(pool_->RegisterFile(file_.TestFileId(), ValidateHeap));
    }

    // Creates `count` raw pages on disk before the pool is registered.
    void AppendRawPages(std::size_t count) {
        for (std::size_t index = 0; index < count; ++index) {
            ASSERT_TRUE(file_.AppendRawHeapPage().has_value());
        }
    }

    [[nodiscard]] FrameSnapshot Frame(PageId page_id) const {
        return FrameOf(*pool_, page_id);
    }

    // Inserts one tuple through a write guard and publishes the mutation.
    static void InsertTuple(WritePageGuard& guard, std::byte value) {
        const auto tuple = heap_test::MakeTestTuple(heap_test::TEST_TUPLE_MIN_SIZE + 3, value);
        HeapPage heap{guard.MutablePage(), heap_test::TestSchemaHistory()};
        ASSERT_TRUE(heap.Insert(tuple));
        guard.MarkDirty();
    }

    HeapFileFixture file_;
    std::unique_ptr<BufferPool> pool_;
};

// ----------------------------------------------------------------------------------------------
// Addressing and registration

TEST_F(BufferPoolTest, RejectsSuperblockUnpublishedAndUnregisteredPages) {
    AppendRawPages(1);
    MakePool(3);

    EXPECT_EQ(pool_->FetchRead(Pid(0)).Error().code, BufferErrorCode::FILE_OR_PAGE_NOT_FOUND);
    EXPECT_EQ(pool_->FetchRead(Pid(2)).Error().code, BufferErrorCode::FILE_OR_PAGE_NOT_FOUND);
    EXPECT_EQ(pool_->FetchRead(PageId{.file_id = FileId{99}, .page_no = 1}).Error().code,
              BufferErrorCode::FILE_OR_PAGE_NOT_FOUND);
    EXPECT_EQ(file_.Io().ReadCount(), 0U);
    EXPECT_FALSE(BufferPoolInspector{*pool_}.InspectPage(Pid(2)).has_value());
}

TEST_F(BufferPoolTest, RegistrationRejectsDuplicates) {
    MakePool(3);
    EXPECT_EQ(pool_->RegisterFile(file_.TestFileId(), ValidateHeap).Error().code,
              BufferErrorCode::INTERNAL_INVARIANT_FAILURE);
}

TEST_F(BufferPoolTest, RegistrationWithoutAValidatorIsAnInternalCheckFailure) {
    MakePool(3);
    EXPECT_DEATH(static_cast<void>(pool_->RegisterFile(FileId{55}, PageValidator{})),
                 "an owner page validator is required");
}

// ----------------------------------------------------------------------------------------------
// Loading and validation before residency

TEST_F(BufferPoolTest, LoadsValidatedPageAsResidentWithOnePin) {
    AppendRawPages(1);
    MakePool(3);

    auto guard = Must(pool_->FetchRead(Pid(1)));
    ASSERT_TRUE(guard.Valid());
    EXPECT_EQ(guard.Id(), Pid(1));
    EXPECT_TRUE(heap_test::TestHeapPage(const_cast<Page&>(guard.GetPage())).Validate());

    const auto entry = EntryOf(*pool_, Pid(1));
    EXPECT_EQ(entry.state, PageTableEntryState::RESIDENT);
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).state, FrameState::RESIDENT);
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).io_state, FrameIoState::NONE);
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).pin_count, 1U);
    EXPECT_FALSE(FrameOf(*pool_, Pid(1)).dirty);
    EXPECT_TRUE(FrameOf(*pool_, Pid(1)).referenced);
    EXPECT_EQ(file_.Io().ReadCount(), 1U);

    guard.Release();
    EXPECT_EQ(Frame(Pid(1)).pin_count, 0U);

    // A hit does not read again.
    auto again = Must(pool_->FetchRead(Pid(1)));
    ASSERT_TRUE(again.Valid());
    EXPECT_EQ(file_.Io().ReadCount(), 1U);
}

struct CorruptionCase {
    const char* name;
    void (*mutate)(Page&);
    BufferErrorCode expected;
};

class BufferPoolCorruptionTest : public BufferPoolTest,
                                 public ::testing::WithParamInterface<CorruptionCase> {};

TEST_P(BufferPoolCorruptionTest, MalformedPagesNeverBecomeResidentAndRetrySucceedsAfterRepair) {
    const PageId page_id = file_.AppendRawHeapPage(GetParam().mutate).value_or(PageId{});
    ASSERT_NE(page_id.file_id, INVALID_FILE_ID);
    MakePool(3);

    const auto failed = pool_->FetchRead(page_id);
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.Error().code, GetParam().expected);
    EXPECT_FALSE(BufferPoolInspector{*pool_}.InspectPage(page_id).has_value());
    EXPECT_EQ(pool_->FreeFrameCount(), 3U);

    // Not poison-cached: repair the fixture and fetch again.
    Page repaired{page_id};
    ASSERT_TRUE(heap_test::TestHeapPage(repaired).Initialize());
    FinalizePageChecksum(repaired.Bytes());
    ASSERT_TRUE(file_.OverwriteRawPage(repaired));
    EXPECT_TRUE(pool_->FetchRead(page_id));
}

INSTANTIATE_TEST_SUITE_P(
    Cases,
    BufferPoolCorruptionTest,
    ::testing::Values(CorruptionCase{"BadChecksum",
                                     [](Page& page) { page.Bytes()[200] ^= std::byte{0x01}; },
                                     BufferErrorCode::CORRUPT_PAGE},
                      CorruptionCase{"WrongPageNumber",
                                     [](Page& page) {
                                         auto header = page.DecodeHeader();
                                         header->page_no = 9;
                                         page.WriteHeader(*header);
                                         FinalizePageChecksum(page.Bytes());
                                     },
                                     BufferErrorCode::CORRUPT_PAGE},
                      CorruptionCase{"OwnerValidationFailure",
                                     [](Page& page) {
                                         auto header = page.DecodeHeader();
                                         header->page_type = PageType::FSM_DATA;
                                         page.WriteHeader(*header);
                                         FinalizePageChecksum(page.Bytes());
                                     },
                                     BufferErrorCode::CORRUPT_PAGE},
                      CorruptionCase{
                          "ZeroPage",
                          [](Page& page) { std::ranges::fill(page.Bytes(), std::byte{0}); },
                          BufferErrorCode::CORRUPT_PAGE}),
    [](const auto& test_info) { return test_info.param.name; });

TEST_F(BufferPoolTest, UnsupportedFormatOutcomeIsDistinctFromCorruption) {
    AppendRawPages(1);
    pool_ = std::make_unique<BufferPool>(file_.Io(), BufferPoolOptions{.frame_count = 2});
    ASSERT_TRUE(pool_->RegisterFile(
        file_.TestFileId(), [](Page&) { return PageValidationOutcome::UNSUPPORTED_FORMAT; }));
    EXPECT_EQ(pool_->FetchRead(Pid(1)).Error().code, BufferErrorCode::UNSUPPORTED_PAGE_FORMAT);
}

TEST_F(BufferPoolTest, ReadFailureIsReportedWithContextAndRetried) {
    AppendRawPages(1);
    MakePool(3);
    file_.Io().FailNextRead();

    const auto failed = pool_->FetchRead(Pid(1));
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.Error().code, BufferErrorCode::RAW_IO_FAILURE);
    EXPECT_EQ(failed.Error().page_id, Pid(1));
    ASSERT_TRUE(failed.Error().disk_error.has_value());
    EXPECT_EQ(failed.Error().disk_error.value_or(DiskError{}).operation, DiskOperation::READ_PAGE);
    EXPECT_EQ(pool_->FreeFrameCount(), 3U);

    EXPECT_TRUE(pool_->FetchRead(Pid(1)));
}

// ----------------------------------------------------------------------------------------------
// Pins and guards

TEST_F(BufferPoolTest, PinCountIsCheckedAtItsMaximumWithoutChangingState) {
    AppendRawPages(1);
    MakePool(3, 2);

    auto first = Must(pool_->FetchRead(Pid(1)));
    auto second = Must(pool_->FetchRead(Pid(1)));
    ASSERT_TRUE(first.Valid());
    ASSERT_TRUE(second.Valid());
    EXPECT_EQ(Frame(Pid(1)).pin_count, 2U);

    const auto overflow = pool_->FetchRead(Pid(1));
    ASSERT_FALSE(overflow);
    EXPECT_EQ(overflow.Error().code, BufferErrorCode::PIN_COUNT_OVERFLOW);
    EXPECT_EQ(Frame(Pid(1)).pin_count, 2U);
    EXPECT_EQ(Frame(Pid(1)).page_id, Pid(1));

    first.Release();
    EXPECT_TRUE(pool_->FetchRead(Pid(1)));
}

TEST_F(BufferPoolTest, GuardMoveTransfersTheSinglePinAndReleaseIsIdempotent) {
    AppendRawPages(2);
    MakePool(3);

    auto fetched = Must(pool_->FetchRead(Pid(1)));
    ASSERT_TRUE(fetched.Valid());
    ReadPageGuard moved{std::move(fetched)};
    EXPECT_FALSE(fetched.Valid()); // NOLINT(bugprone-use-after-move): verifies the inert source
    EXPECT_TRUE(moved.Valid());
    EXPECT_EQ(Frame(Pid(1)).pin_count, 1U);

    fetched.Release(); // inert source: no effect
    EXPECT_EQ(Frame(Pid(1)).pin_count, 1U);

    moved.Release();
    moved.Release();
    EXPECT_EQ(Frame(Pid(1)).pin_count, 0U);

    // Move assignment releases the destination's previous pin.
    auto a = Must(pool_->FetchWrite(Pid(1)));
    auto b = Must(pool_->FetchWrite(Pid(2)));
    ASSERT_TRUE(a.Valid());
    ASSERT_TRUE(b.Valid());
    a = std::move(b);
    EXPECT_EQ(Frame(Pid(1)).pin_count, 0U);
    EXPECT_EQ(Frame(Pid(2)).pin_count, 1U);
    EXPECT_EQ(a.Id(), Pid(2));
}

TEST_F(BufferPoolTest, WriteGuardAloneDoesNotDirtyAndMarkDirtyAdvancesGeneration) {
    AppendRawPages(1);
    MakePool(3);

    const auto before = [&] {
        auto guard = pool_->FetchWrite(Pid(1));
        EXPECT_TRUE(guard);
        return Frame(Pid(1)).modification_generation;
    }();
    EXPECT_FALSE(Frame(Pid(1)).dirty);

    {
        auto guard = Must(pool_->FetchWrite(Pid(1)));
        ASSERT_TRUE(guard.Valid());
        InsertTuple(guard, std::byte{1});
        InsertTuple(guard, std::byte{2});
    }
    const auto after = Frame(Pid(1));
    EXPECT_TRUE(after.dirty);
    EXPECT_EQ(after.modification_generation, before + 2U);
}

#ifndef NDEBUG
TEST_F(BufferPoolTest, UnreportedMutationIsAnInvariantViolation) {
    AppendRawPages(1);
    MakePool(3);
    EXPECT_DEATH(
        {
            auto guard = Must(pool_->FetchWrite(Pid(1)));
            guard.MutableBytes()[500] = std::byte{7};
            guard.Release();
        },
        "MarkDirty\\(\\) did not publish");
}
#endif

// ----------------------------------------------------------------------------------------------
// New pages

TEST_F(BufferPoolTest, NewPageIsConstructedWithoutReadingAndPublishedWithOneCreatorPin) {
    MakePool(3);
    auto created = Must(pool_->NewPage(file_.TestFileId(), InitializeHeap));
    ASSERT_TRUE(created.Valid());
    EXPECT_EQ(created.Id(), Pid(1));
    EXPECT_EQ(file_.Io().ReadCount(), 0U);

    const auto frame = Frame(Pid(1));
    EXPECT_EQ(frame.state, FrameState::RESIDENT);
    EXPECT_EQ(frame.pin_count, 1U);
    EXPECT_TRUE(frame.dirty);
    EXPECT_GT(frame.modification_generation, 0U);

    InsertTuple(created, std::byte{9});
    created.Release();

    auto fetched = Must(pool_->FetchRead(Pid(1)));
    ASSERT_TRUE(fetched.Valid());
    EXPECT_TRUE(
        heap_test::TestHeapPage(const_cast<Page&>(fetched.GetPage())).TupleBytes(0).has_value());
}

TEST_F(BufferPoolTest, NewPageWithNoReplaceableFrameDoesNotExtendTheFile) {
    MakePool(1);
    auto held = Must(pool_->NewPage(file_.TestFileId(), InitializeHeap));
    ASSERT_TRUE(held.Valid());
    const auto extends = file_.Io().ExtendCount();

    const auto failed = pool_->NewPage(file_.TestFileId(), InitializeHeap);
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.Error().code, BufferErrorCode::NO_REPLACEABLE_FRAME);
    EXPECT_EQ(file_.Io().ExtendCount(), extends);
}

TEST_F(BufferPoolTest, InvalidInitializerAndExtensionFailureLeaveNoMappingOrFrame) {
    MakePool(2);

    file_.Io().FailNextExtend();
    const auto extend_failed = pool_->NewPage(file_.TestFileId(), InitializeHeap);
    EXPECT_EQ(extend_failed.Error().code, BufferErrorCode::RAW_IO_FAILURE);
    EXPECT_EQ(pool_->FreeFrameCount(), 2U);

    const auto invalid = pool_->NewPage(file_.TestFileId(), [](Page&) { return true; });
    ASSERT_FALSE(invalid);
    EXPECT_EQ(invalid.Error().code, BufferErrorCode::INTERNAL_INVARIANT_FAILURE);
    EXPECT_FALSE(BufferPoolInspector{*pool_}.InspectPage(invalid.Error().page_id).has_value());
    EXPECT_EQ(pool_->FreeFrameCount(), 2U);
    // The unpublished page is not fetchable.
    EXPECT_EQ(pool_->FetchRead(invalid.Error().page_id).Error().code,
              BufferErrorCode::FILE_OR_PAGE_NOT_FOUND);

    // The unpublished tail PageNo is reused rather than extending again.
    const auto extends = file_.Io().ExtendCount();
    auto created = Must(pool_->NewPage(file_.TestFileId(), InitializeHeap));
    ASSERT_TRUE(created.Valid());
    EXPECT_EQ(created.Id(), invalid.Error().page_id);
    EXPECT_EQ(file_.Io().ExtendCount(), extends);
}

// ----------------------------------------------------------------------------------------------
// Copied stable flush

TEST_F(BufferPoolTest, FlushWritesAChecksummedImageAndPublishesClean) {
    MakePool(3);
    {
        auto created = Must(pool_->NewPage(file_.TestFileId(), InitializeHeap));
        ASSERT_TRUE(created.Valid());
        InsertTuple(created, std::byte{0x5A});
    }
    ASSERT_TRUE(pool_->FlushPage(Pid(1)));
    EXPECT_EQ(file_.Io().WriteCount(), 1U);
    EXPECT_EQ(file_.Io().SyncCount(), 1U);
    EXPECT_FALSE(Frame(Pid(1)).dirty);
    EXPECT_EQ(Frame(Pid(1)).page_id, Pid(1)); // flush keeps the residency

    auto on_disk = file_.ReadRawPage(Pid(1));
    EXPECT_TRUE(VerifyPageChecksum(on_disk.Bytes()));
    EXPECT_EQ(buffer_test::FirstTupleByte(on_disk), std::byte{0x5A});

    // Already clean: no I/O.
    ASSERT_TRUE(pool_->FlushPage(Pid(1)));
    ASSERT_TRUE(pool_->FlushAll());
    EXPECT_EQ(file_.Io().WriteCount(), 1U);
}

TEST_F(BufferPoolTest, FlushingAPinnedPageKeepsItResidentAndPinned) {
    MakePool(3);
    {
        auto created = Must(pool_->NewPage(file_.TestFileId(), InitializeHeap));
        ASSERT_TRUE(created.Valid());
    }
    auto reader = Must(pool_->FetchRead(Pid(1)));
    ASSERT_TRUE(reader.Valid());
    std::thread flusher{[&] { EXPECT_TRUE(pool_->FlushPage(Pid(1))); }};
    flusher.join();
    EXPECT_FALSE(Frame(Pid(1)).dirty);
    EXPECT_EQ(Frame(Pid(1)).pin_count, 1U);
}

struct FlushFaultCase {
    const char* name;
    WriteFault write_fault;
    bool sync_fault;
};

class BufferPoolFlushFaultTest : public BufferPoolTest,
                                 public ::testing::WithParamInterface<FlushFaultCase> {};

TEST_P(BufferPoolFlushFaultTest, FailedWritebackKeepsTheFrameDirtyAndRetryable) {
    MakePool(3);
    {
        auto created = Must(pool_->NewPage(file_.TestFileId(), InitializeHeap));
        ASSERT_TRUE(created.Valid());
        InsertTuple(created, std::byte{3});
    }
    const auto before = Frame(Pid(1));
    if (GetParam().write_fault != WriteFault::NONE) {
        file_.Io().FailNextWrite(GetParam().write_fault);
    }
    if (GetParam().sync_fault) {
        file_.Io().FailNextSync();
    }

    const auto failed = pool_->FlushPage(Pid(1));
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.Error().code, BufferErrorCode::RAW_IO_FAILURE);
    const auto after = Frame(Pid(1));
    EXPECT_TRUE(after.dirty);
    EXPECT_EQ(after.modification_generation, before.modification_generation);
    EXPECT_EQ(after.io_state, FrameIoState::NONE);
    EXPECT_EQ(after.page_id, Pid(1));

    ASSERT_TRUE(pool_->FlushPage(Pid(1)));
    EXPECT_FALSE(Frame(Pid(1)).dirty);
    auto on_disk = file_.ReadRawPage(Pid(1));
    EXPECT_TRUE(VerifyPageChecksum(on_disk.Bytes()));
}

INSTANTIATE_TEST_SUITE_P(Cases,
                         BufferPoolFlushFaultTest,
                         ::testing::Values(FlushFaultCase{"ZeroByteWrite", WriteFault::FAIL, false},
                                           FlushFaultCase{"ShortWrite", WriteFault::SHORT, false},
                                           FlushFaultCase{
                                               "SyncAfterFullWrite", WriteFault::NONE, true}),
                         [](const auto& test_info) { return test_info.param.name; });

// ----------------------------------------------------------------------------------------------
// CLOCK, eviction, and reset

TEST_F(BufferPoolTest, CleanVictimIsEvictedWithoutWritingAndFullyReset) {
    AppendRawPages(4);
    MakePool(3);
    for (PageNo page_no = 1; page_no <= 3; ++page_no) {
        ASSERT_TRUE(pool_->FetchRead(Pid(page_no)));
    }
    const auto victim_frame = Frame(Pid(1));

    ASSERT_TRUE(pool_->FetchRead(Pid(4)));
    EXPECT_EQ(file_.Io().WriteCount(), 0U);

    // All three were referenced; the second-chance sweep clears them and returns to frame 0.
    EXPECT_EQ(FrameOf(*pool_, Pid(4)).frame_id, victim_frame.frame_id);
    EXPECT_FALSE(BufferPoolInspector{*pool_}.InspectPage(Pid(1)).has_value());
    EXPECT_EQ(FrameOf(*pool_, Pid(4)).binding_epoch, victim_frame.binding_epoch + 1U);
    EXPECT_EQ(FrameOf(*pool_, Pid(4)).modification_generation,
              victim_frame.modification_generation);
    EXPECT_FALSE(FrameOf(*pool_, Pid(4)).dirty);
}

TEST_F(BufferPoolTest, DirtyVictimIsStablyWrittenBeforeReuse) {
    MakePool(2);
    for (int index = 0; index < 2; ++index) {
        auto created = Must(pool_->NewPage(file_.TestFileId(), InitializeHeap));
        ASSERT_TRUE(created.Valid());
        InsertTuple(created, static_cast<std::byte>(index + 1));
    }
    ASSERT_EQ(file_.Io().WriteCount(), 0U);

    {
        auto third = Must(pool_->NewPage(file_.TestFileId(), InitializeHeap));
        ASSERT_TRUE(third.Valid());
    }
    EXPECT_EQ(file_.Io().WriteCount(), 1U);
    EXPECT_EQ(file_.Io().SyncCount(), 1U);
    EXPECT_FALSE(BufferPoolInspector{*pool_}.InspectPage(Pid(1)).has_value());

    auto on_disk = file_.ReadRawPage(Pid(1));
    EXPECT_TRUE(VerifyPageChecksum(on_disk.Bytes()));
    EXPECT_EQ(buffer_test::FirstTupleByte(on_disk), std::byte{1});

    // The written page reloads intact.
    auto reloaded = Must(pool_->FetchRead(Pid(1)));
    ASSERT_TRUE(reloaded.Valid());
    EXPECT_EQ(buffer_test::FirstTupleByte(reloaded.GetPage()), std::byte{1});
}

TEST_F(BufferPoolTest, FailedEvictionWritebackRestoresTheOldPageAndFailsTheRequester) {
    AppendRawPages(1);
    MakePool(1);
    {
        auto guard = Must(pool_->FetchWrite(Pid(1)));
        ASSERT_TRUE(guard.Valid());
        InsertTuple(guard, std::byte{4});
    }
    const auto before = Frame(Pid(1));
    file_.Io().FailNextSync();

    const auto failed = pool_->NewPage(file_.TestFileId(), InitializeHeap);
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.Error().code, BufferErrorCode::RAW_IO_FAILURE);

    const auto restored = EntryOf(*pool_, Pid(1));
    EXPECT_EQ(restored.state, PageTableEntryState::RESIDENT);
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).state, FrameState::RESIDENT);
    EXPECT_TRUE(FrameOf(*pool_, Pid(1)).dirty);
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).modification_generation, before.modification_generation);
    EXPECT_EQ(FrameOf(*pool_, Pid(1)).binding_epoch, before.binding_epoch);

    // Pinnable again, and a later independent request succeeds.
    EXPECT_TRUE(pool_->FetchRead(Pid(1)));
    EXPECT_TRUE(pool_->NewPage(file_.TestFileId(), InitializeHeap));
}

TEST_F(BufferPoolTest, OneUnsuccessfulClockPassReturnsNoReplaceableFrame) {
    AppendRawPages(4);
    MakePool(3);
    std::vector<ReadPageGuard> held;
    for (PageNo page_no = 1; page_no <= 3; ++page_no) {
        auto guard = Must(pool_->FetchRead(Pid(page_no)));
        ASSERT_TRUE(guard.Valid());
        held.push_back(std::move(guard));
    }

    const auto failed = pool_->FetchRead(Pid(4));
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.Error().code, BufferErrorCode::NO_REPLACEABLE_FRAME);
    EXPECT_EQ(BufferPoolInspector{*pool_}.LastClockPassVisits(), 6U);
    EXPECT_FALSE(BufferPoolInspector{*pool_}.InspectPage(Pid(4)).has_value());
    for (PageNo page_no = 1; page_no <= 3; ++page_no) {
        EXPECT_EQ(Frame(Pid(page_no)).pin_count, 1U);
        EXPECT_EQ(Frame(Pid(page_no)).state, FrameState::RESIDENT);
    }

    // Releasing one guard makes its frame replaceable. The fetch runs on another thread so this
    // thread never takes a page latch while holding others out of frame order.
    held.erase(held.begin() + 1);
    std::thread fetcher{[&] { EXPECT_TRUE(pool_->FetchRead(Pid(4))); }};
    fetcher.join();
    EXPECT_FALSE(BufferPoolInspector{*pool_}.InspectPage(Pid(2)).has_value());
}

TEST_F(BufferPoolTest, RecentlyReferencedPagesSurviveTheSecondChanceSweep) {
    AppendRawPages(5);
    MakePool(3);
    for (PageNo page_no = 1; page_no <= 3; ++page_no) {
        ASSERT_TRUE(pool_->FetchRead(Pid(page_no)));
    }
    ASSERT_TRUE(pool_->FetchRead(Pid(4))); // sweep clears all bits, evicts page 1, hand at frame 1
    ASSERT_TRUE(pool_->FetchRead(Pid(2))); // re-reference page 2 in frame 1
    ASSERT_TRUE(pool_->FetchRead(Pid(5))); // frame 1 gets its second chance; page 3 is evicted
    EXPECT_TRUE(BufferPoolInspector{*pool_}.InspectPage(Pid(2)).has_value());
    EXPECT_FALSE(BufferPoolInspector{*pool_}.InspectPage(Pid(3)).has_value());
}

TEST_F(BufferPoolTest, StaleViewIsDetectableAfterReassignment) {
    AppendRawPages(2);
    MakePool(1);
    FrameId frame_id = 0;
    std::uint64_t epoch = 0;
    {
        auto guard = Must(pool_->FetchRead(Pid(1)));
        ASSERT_TRUE(guard.Valid());
        frame_id = guard.Frame();
        epoch = guard.BindingEpoch();
        EXPECT_TRUE(BufferPoolInspector{*pool_}.IsCurrentBinding(frame_id, Pid(1), epoch));
    }
    ASSERT_TRUE(pool_->FetchRead(Pid(2)));
    EXPECT_FALSE(BufferPoolInspector{*pool_}.IsCurrentBinding(frame_id, Pid(1), epoch));
    EXPECT_EQ(Frame(Pid(2)).frame_id, frame_id);
}

// ----------------------------------------------------------------------------------------------
// File retirement and shutdown

TEST_F(BufferPoolTest, RetirementFlushesDirtyPagesAndClosesTheRegistration) {
    MakePool(3);
    {
        auto created = Must(pool_->NewPage(file_.TestFileId(), InitializeHeap));
        ASSERT_TRUE(created.Valid());
        InsertTuple(created, std::byte{8});
    }
    ASSERT_TRUE(pool_->RetireFile(file_.TestFileId(), RetireDisposition::FLUSH_DIRTY));
    EXPECT_EQ(file_.Io().WriteCount(), 1U);
    EXPECT_FALSE(BufferPoolInspector{*pool_}.InspectPage(Pid(1)).has_value());
    EXPECT_EQ(pool_->FreeFrameCount(), 3U);
    EXPECT_EQ(pool_->FetchRead(Pid(1)).Error().code, BufferErrorCode::FILE_OR_PAGE_NOT_FOUND);
#ifndef NDEBUG
    // Reset frames carry the debug poison pattern.
    EXPECT_EQ(BufferPoolInspector{*pool_}.InspectFrame(0).state, FrameState::FREE);
#endif
}

TEST_F(BufferPoolTest, DiscardRetirementWritesNothing) {
    MakePool(3);
    {
        auto created = Must(pool_->NewPage(file_.TestFileId(), InitializeHeap));
        ASSERT_TRUE(created.Valid());
    }
    ASSERT_TRUE(pool_->RetireFile(file_.TestFileId(), RetireDisposition::DISCARD_DIRTY));
    EXPECT_EQ(file_.Io().WriteCount(), 0U);
    EXPECT_EQ(pool_->FreeFrameCount(), 3U);
}

TEST_F(BufferPoolTest, RetirementWaitsForExistingGuardsAndRejectsNewClaims) {
    AppendRawPages(2);
    MakePool(3);
    auto held = Must(pool_->FetchRead(Pid(1)));
    ASSERT_TRUE(held.Valid());

    std::atomic<bool> retired{false};
    std::thread retirer{[&] {
        EXPECT_TRUE(pool_->RetireFile(file_.TestFileId(), RetireDisposition::FLUSH_DIRTY));
        retired = true;
    }};
    // Wait until the gate is visible: a new claim is rejected.
    WaitFor([&] {
        return ErrorCodeOf(pool_->FetchRead(Pid(2))) == BufferErrorCode::FILE_RETIRED_OR_CLOSING;
    });
    EXPECT_EQ(pool_->NewPage(file_.TestFileId(), InitializeHeap).Error().code,
              BufferErrorCode::FILE_RETIRED_OR_CLOSING);
    // The existing guard is still valid while retirement drains.
    EXPECT_FALSE(retired.load());
    EXPECT_TRUE(heap_test::TestHeapPage(const_cast<Page&>(held.GetPage())).Validate());

    held.Release();
    retirer.join();
    EXPECT_TRUE(retired.load());
    EXPECT_FALSE(BufferPoolInspector{*pool_}.InspectPage(Pid(1)).has_value());
}

TEST_F(BufferPoolTest, FailedRetirementFlushLeavesTheFileRetiringAndRetryable) {
    MakePool(3);
    {
        auto created = Must(pool_->NewPage(file_.TestFileId(), InitializeHeap));
        ASSERT_TRUE(created.Valid());
    }
    file_.Io().FailNextWrite(WriteFault::FAIL);
    const auto failed = pool_->RetireFile(file_.TestFileId(), RetireDisposition::FLUSH_DIRTY);
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.Error().code, BufferErrorCode::RAW_IO_FAILURE);
    EXPECT_TRUE(Frame(Pid(1)).dirty);
    EXPECT_EQ(pool_->FetchRead(Pid(1)).Error().code, BufferErrorCode::FILE_RETIRED_OR_CLOSING);

    ASSERT_TRUE(pool_->RetireFile(file_.TestFileId(), RetireDisposition::FLUSH_DIRTY));
    EXPECT_FALSE(BufferPoolInspector{*pool_}.InspectPage(Pid(1)).has_value());
}

TEST_F(BufferPoolTest, ShutdownDrainsGuardsFlushesDirtyPagesAndRejectsNewWork) {
    AppendRawPages(1);
    MakePool(3);
    {
        auto guard = Must(pool_->FetchWrite(Pid(1)));
        ASSERT_TRUE(guard.Valid());
        InsertTuple(guard, std::byte{6});
    }
    auto held = Must(pool_->FetchRead(Pid(1)));
    ASSERT_TRUE(held.Valid());

    std::atomic<bool> finished{false};
    std::thread shutdown{[&] {
        EXPECT_TRUE(pool_->Shutdown());
        finished = true;
    }};
    WaitFor([&] {
        return ErrorCodeOf(pool_->FetchRead(Pid(1))) == BufferErrorCode::BUFFERPOOL_QUIESCING;
    });
    EXPECT_EQ(pool_->NewPage(file_.TestFileId(), InitializeHeap).Error().code,
              BufferErrorCode::BUFFERPOOL_QUIESCING);
    EXPECT_FALSE(finished.load());

    held.Release();
    shutdown.join();
    EXPECT_FALSE(Frame(Pid(1)).dirty);
    auto on_disk = file_.ReadRawPage(Pid(1));
    EXPECT_EQ(buffer_test::FirstTupleByte(on_disk), std::byte{6});
}

TEST_F(BufferPoolTest, ShutdownFlushFailureIsReported) {
    MakePool(3);
    {
        auto created = Must(pool_->NewPage(file_.TestFileId(), InitializeHeap));
        ASSERT_TRUE(created.Valid());
    }
    file_.Io().FailNextSync();
    const auto failed = pool_->Shutdown();
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.Error().code, BufferErrorCode::RAW_IO_FAILURE);
    EXPECT_TRUE(Frame(Pid(1)).dirty);
}

} // namespace
} // namespace dblusblus::storage
