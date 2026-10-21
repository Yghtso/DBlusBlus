#pragma once

#include "common/types.h"
#include "storage/buffer/buffer_error.h"
#include "storage/buffer/buffer_frame.h"
#include "storage/buffer/clock_replacer.h"
#include "storage/buffer/page_guard.h"
#include "storage/buffer/page_io.h"
#include "storage/disk/disk_manager.h"
#include "storage/page/page.h"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace dblusblus::storage {

// Result of a registered file owner's page-local validation (ARCHITECTURE.md §7.6.4).
enum class PageValidationOutcome : std::uint8_t {
    VALID,
    CORRUPT,
    UNSUPPORTED_FORMAT,
};

// Bounded, nonmutating, nonfetching owner validation run while the frame is still LOADING. The
// Page is passed mutably only because page controllers such as HeapPage take Page&; validators
// must not change it. It must not call back into the BufferPool.
using PageValidator = std::function<PageValidationOutcome(Page& page)>;

// Builds the canonical initial image of a newly allocated page. The page arrives zero-filled with
// its PageId bound. Returns false if construction failed.
using PageInitializer = std::function<bool(Page& page)>;

// What RetireFile does with drained dirty frames (ARCHITECTURE.md §7.12.5).
enum class RetireDisposition : std::uint8_t {
    // Stable-flush every dirty frame before closing. Ordinary file close.
    FLUSH_DIRTY,
    // Discard dirty frames without writeback. Only for a semantic owner that has proven the file
    // obsolete (drop/orphan retirement).
    DISCARD_DIRTY,
};

enum class PageTableEntryState : std::uint8_t {
    LOAD_INTENT,
    LOADING,
    RESIDENT,
    EVICTING,
};

// Semantic events exposed to a test observer (VERIFICATION.md "Deterministic harness"). The
// observer is always invoked without the pool mutex held, so it may block to build deterministic
// interleavings. It must not call BufferPool methods that would wait on the blocked thread.
enum class BufferPoolEventType : std::uint8_t {
    LOAD_INTENT_INSTALLED,
    FRAME_BOUND_LOADING,
    LOAD_PUBLISHED,
    LOAD_FAILED,
    HIT_PIN_ATTEMPT,
    VICTIM_CANDIDATE_OBSERVED,
    VICTIM_RESERVED,
    WRITEBACK_IMAGE_COPIED,
    WRITEBACK_WRITE_STARTED,
    WRITEBACK_SYNC_STARTED,
    WRITEBACK_RECONCILE_STARTED,
    GUARD_LATCH_RELEASED,
    FILE_RETIRING,
    FILE_CLOSED,
};

struct BufferPoolEvent {
    BufferPoolEventType type{BufferPoolEventType::LOAD_INTENT_INSTALLED};
    PageId page_id{};
    std::optional<FrameId> frame_id;
};

class BufferPoolObserver {
  public:
    BufferPoolObserver() = default;
    virtual ~BufferPoolObserver() = default;

    BufferPoolObserver(const BufferPoolObserver&) = delete;
    BufferPoolObserver& operator=(const BufferPoolObserver&) = delete;
    BufferPoolObserver(BufferPoolObserver&&) = delete;
    BufferPoolObserver& operator=(BufferPoolObserver&&) = delete;

    virtual void OnEvent(const BufferPoolEvent& event) = 0;
};

struct BufferPoolOptions {
    std::size_t frame_count{0};
    // Checked upper bound for pins and pending fetch claims on one frame. Tests lower it to reach
    // the overflow boundary deterministically.
    std::uint32_t max_pin_count{std::numeric_limits<std::uint32_t>::max()};
    // Optional, non-owning; must outlive the pool.
    BufferPoolObserver* observer{nullptr};
};

// Process-local cache of database pages (ARCHITECTURE.md Chapter 7).
//
// The BufferPool owns the PageId -> frame binding, pins, page latches, dirty state, CLOCK
// replacement, eviction, and copied stable writeback. It is format agnostic: page-family rules are
// supplied by the validator registered for each file.
//
// Synchronization: one pool mutex protects the page table, file registry, frame metadata, and the
// CLOCK hand. Disk I/O, owner validation, page initialization, and page-latch waits run with the
// mutex released. A page latch may be acquired before the mutex but never while holding it.
//
// Callers must not flush, retire, or shut down while the same thread holds a guard on an affected
// page: those operations wait for latches or pins that thread would never release.
class BufferPool {
  public:
    // The DiskManager must outlive the pool.
    BufferPool(DiskManager& disk_manager, BufferPoolOptions options);
    // The PageIo must outlive the pool.
    BufferPool(PageIo& page_io, BufferPoolOptions options);
    // Performs no I/O. Dirty pages still resident are lost unless Shutdown() or RetireFile() ran
    // first. Destroying a pool with outstanding pins is an invariant violation.
    ~BufferPool();

    BufferPool(const BufferPool&) = delete;
    BufferPool& operator=(const BufferPool&) = delete;
    BufferPool(BufferPool&&) = delete;
    BufferPool& operator=(BufferPool&&) = delete;

    // Makes a FileId already registered with the raw I/O layer fetchable through this pool. The
    // published page bound starts at the file's current page count. Page 0 (the superblock) is
    // never served by ordinary fetch; it stays on the PageFile open path.
    [[nodiscard]] BufferStatus RegisterFile(FileId file_id, PageValidator validator);

    // ACTIVE -> RETIRING -> CLOSED (ARCHITECTURE.md §7.12.5). New fetches fail immediately;
    // existing guards and I/O drain; dirty frames are flushed or discarded; then every frame for
    // the file is reset and the registration removed. If flushing fails the file stays RETIRING and
    // the call may be retried.
    [[nodiscard]] BufferStatus RetireFile(FileId file_id, RetireDisposition disposition);

    [[nodiscard]] BufferResult<ReadPageGuard> FetchRead(PageId page_id);
    [[nodiscard]] BufferResult<WritePageGuard> FetchWrite(PageId page_id);

    // Appends one page to the file, constructs it in a frame without reading disk bytes, validates
    // it, and publishes it RESIDENT and dirty together with the advanced page bound. Returns the
    // creator's write guard.
    [[nodiscard]] BufferResult<WritePageGuard> NewPage(FileId file_id,
                                                       const PageInitializer& initializer);

    // Explicit "flush current contents": repeats copied writeback until the then-current generation
    // is stably clean. A nonresident or clean page succeeds without I/O.
    [[nodiscard]] BufferStatus FlushPage(PageId page_id);
    [[nodiscard]] BufferStatus FlushFile(FileId file_id);
    [[nodiscard]] BufferStatus FlushAll();

    // Quiesces the pool: rejects new fetch/new-page work, waits for guards and I/O to drain, then
    // stably flushes every dirty page. On failure the pool stays quiescing and reports the error.
    [[nodiscard]] BufferStatus Shutdown();

    [[nodiscard]] std::size_t FrameCount() const noexcept {
        return frame_count_;
    }
    [[nodiscard]] std::size_t FreeFrameCount() const;

  private:
    friend class detail::PageGuardBase;
    friend class WritePageGuard;
    // Test-only read access to frame and page-table state (buffer_pool_inspector.h).
    friend class BufferPoolInspector;

    enum class FileState : std::uint8_t {
        ACTIVE,
        RETIRING,
    };

    struct FileEntry {
        FileState state{FileState::ACTIVE};
        PageValidator validator;
        PageNo published_page_count{0};
        bool new_page_in_progress{false};
    };

    // One in-progress load shared by its loader and every joined fetch.
    struct LoadOperation {
        std::uint32_t claims{0};
        bool done{false};
        std::optional<FrameId> frame_id;
        BufferError error;
    };

    struct PageTableEntry {
        PageTableEntryState state{PageTableEntryState::LOAD_INTENT};
        FrameId frame_id{0};
        std::shared_ptr<LoadOperation> load;
    };

    enum class LatchMode : std::uint8_t {
        SHARED,
        EXCLUSIVE,
    };

    BufferPool(PageIo* page_io, std::unique_ptr<PageIo> owned_io, BufferPoolOptions options);

    // Returns the pinned frame for page_id, loading it when needed.
    [[nodiscard]] BufferResult<FrameId> PinPage(PageId page_id);
    // Fails unless page_id is an ordinary page of an active registered file. Requires the lock.
    [[nodiscard]] BufferStatus CheckFetchableLocked(PageId page_id) const;
    [[nodiscard]] BufferResult<FrameId> PinResidentLocked(FrameId frame_id, PageId page_id);
    // Adds a claim to another fetch's in-progress load and waits for its result.
    [[nodiscard]] BufferResult<FrameId> JoinLoad(std::unique_lock<std::mutex>& lock,
                                                 PageId page_id,
                                                 std::shared_ptr<LoadOperation> load);
    [[nodiscard]] BufferResult<FrameId> LoadPage(std::unique_lock<std::mutex>& lock,
                                                 PageId page_id);
    // The error a finished read and validation turn into, if any. Requires the lock.
    [[nodiscard]] std::optional<BufferError>
    LoadFailureLocked(PageId page_id, const DiskStatus& read, PageValidationOutcome outcome) const;
    // Waits until no other NewPage is running for file_id. Fails if the file or pool stops
    // accepting work meanwhile. Requires the lock.
    [[nodiscard]] BufferStatus WaitForNewPageTurn(std::unique_lock<std::mutex>& lock,
                                                  FileId file_id);
    // Picks the PageNo for a new page: first_unpublished (the page just past the published bound)
    // if an earlier failed creation left it on disk, else a newly appended page. Called without
    // the lock.
    [[nodiscard]] DiskResult<PageNo> ReserveNewPageNo(PageId first_unpublished);
    void PublishLoadLocked(std::unique_lock<std::mutex>& lock,
                           PageId page_id,
                           FrameId frame_id,
                           LoadOperation& load);
    // Obtains a FREE frame owned exclusively by the caller: from the free list or by evicting a
    // CLOCK victim. May release the lock during eviction writeback.
    [[nodiscard]] BufferResult<FrameId>
    AcquireFrame(std::unique_lock<std::mutex>& lock, PageId requester, BufferOperation operation);
    [[nodiscard]] bool IsVictimEligible(FrameId frame_id) const;
    [[nodiscard]] static PageValidationOutcome
    ValidateLoadedPage(BufferFrame& frame, const PageValidator& validator, bool verify_checksum);
    // Copied stable writeback of one RESIDENT frame whose writeback reservation the caller holds.
    // Returns with the lock held and the reservation released.
    [[nodiscard]] BufferStatus WritebackResident(std::unique_lock<std::mutex>& lock,
                                                 FrameId frame_id,
                                                 BufferOperation operation);
    // Writes and synchronizes a private image. Called without the lock.
    [[nodiscard]] BufferStatus WriteStableImage(PageId page_id,
                                                std::optional<FrameId> frame_id,
                                                std::span<std::byte, PAGE_SIZE> image,
                                                BufferOperation operation);
    [[nodiscard]] BufferStatus
    FlushPageLocked(std::unique_lock<std::mutex>& lock, PageId page_id, BufferOperation operation);
    [[nodiscard]] BufferStatus FlushMatching(std::unique_lock<std::mutex>& lock,
                                             std::optional<FileId> file_id,
                                             BufferOperation operation);
    void ResetFrame(FrameId frame_id);
    void ReleaseFrameToFreeList(FrameId frame_id);
    void Unpin(FrameId frame_id) noexcept;
    void PublishMutation(FrameId frame_id) noexcept;
    void Notify(std::unique_lock<std::mutex>& lock, const BufferPoolEvent& event);
    void NotifyUnlocked(const BufferPoolEvent& event);

    template <typename Guard>
    [[nodiscard]] BufferResult<Guard> MakeGuard(FrameId frame_id, LatchMode mode);

    std::unique_ptr<PageIo> owned_io_;
    PageIo* io_;
    std::size_t frame_count_;
    std::uint32_t max_pin_count_;
    BufferPoolObserver* observer_;

    mutable std::mutex mutex_;
    std::condition_variable state_changed_;
    // Sized once at construction and never resized: BufferFrame holds a latch and cannot move.
    std::vector<BufferFrame> frames_;
    std::vector<FrameId> free_list_;
    ClockReplacer replacer_;
    std::unordered_map<PageId, PageTableEntry> page_table_;
    std::unordered_map<FileId, FileEntry> files_;
    bool quiescing_{false};
};

} // namespace dblusblus::storage
