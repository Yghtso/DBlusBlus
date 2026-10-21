#ifndef DBLUSBLUS_TESTS_STORAGE_BUFFER_TEST_SUPPORT_H_
#define DBLUSBLUS_TESTS_STORAGE_BUFFER_TEST_SUPPORT_H_

#include "storage/buffer/buffer_pool.h"
#include "storage/buffer/page_io.h"
#include "storage/disk/disk_manager.h"
#include "storage/file/file_superblock.h"
#include "storage/file/page_file.h"
#include "storage/heap/heap_page.h"
#include "storage/page/page.h"
#include "storage/page/page_checksum.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <gtest/gtest.h>
#include <mutex>
#include <optional>
#include <string_view>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace dblusblus::buffer_test {

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        std::array path_template{'/', 't', 'm', 'p', '/', 'd', 'b', 'l', 'u', 's', 'b', 'l', 'u',
                                 's', '-', 'b', 'u', 'f', '-', 'X', 'X', 'X', 'X', 'X', 'X', '\0'};
        if (char* created = ::mkdtemp(path_template.data()); created != nullptr) {
            path_ = created;
        }
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

    [[nodiscard]] bool valid() const noexcept {
        return !path_.empty();
    }

    [[nodiscard]] std::filesystem::path File(std::string_view name) const {
        return path_ / name;
    }

  private:
    std::filesystem::path path_;
};

enum class WriteFault : std::uint8_t {
    NONE,
    FAIL,  // nothing transferred
    SHORT, // half the page reaches the file, then the transfer reports failure
};

// Wraps the real DiskManager path and injects one-shot faults. Thread-safe.
class FaultInjectingPageIo final : public PageIo {
  public:
    explicit FaultInjectingPageIo(DiskManager& disk_manager) : inner_(disk_manager) {}

    void FailNextRead() {
        const std::scoped_lock lock{mutex_};
        fail_next_read_ = true;
    }
    void FailNextWrite(WriteFault fault) {
        const std::scoped_lock lock{mutex_};
        next_write_fault_ = fault;
    }
    void FailNextSync() {
        const std::scoped_lock lock{mutex_};
        fail_next_sync_ = true;
    }
    void FailNextExtend() {
        const std::scoped_lock lock{mutex_};
        fail_next_extend_ = true;
    }

    [[nodiscard]] std::size_t reads() const noexcept {
        return reads_.load();
    }
    [[nodiscard]] std::size_t writes() const noexcept {
        return writes_.load();
    }
    [[nodiscard]] std::size_t syncs() const noexcept {
        return syncs_.load();
    }
    [[nodiscard]] std::size_t extends() const noexcept {
        return extends_.load();
    }

    [[nodiscard]] DiskStatus ReadPage(PageId page_id,
                                      std::span<std::byte, PAGE_SIZE> output) override {
        ++reads_;
        if (TakeFlag(fail_next_read_)) {
            return Injected(page_id, DiskOperation::READ_PAGE);
        }
        return inner_.ReadPage(page_id, output);
    }

    [[nodiscard]] DiskStatus WritePage(PageId page_id,
                                       std::span<const std::byte, PAGE_SIZE> data) override {
        ++writes_;
        WriteFault fault = WriteFault::NONE;
        {
            const std::scoped_lock lock{mutex_};
            fault = std::exchange(next_write_fault_, WriteFault::NONE);
        }
        if (fault == WriteFault::FAIL) {
            return Injected(page_id, DiskOperation::WRITE_PAGE);
        }
        if (fault == WriteFault::SHORT) {
            std::array<std::byte, PAGE_SIZE> torn{};
            std::ranges::copy(data.first(PAGE_SIZE / 2U), torn.begin());
            static_cast<void>(inner_.WritePage(page_id, torn));
            return Injected(page_id, DiskOperation::WRITE_PAGE);
        }
        return inner_.WritePage(page_id, data);
    }

    [[nodiscard]] DiskStatus SyncFile(FileId file_id) override {
        ++syncs_;
        if (TakeFlag(fail_next_sync_)) {
            return Injected(PageId{.file_id = file_id, .page_no = INVALID_PAGE_NO},
                            DiskOperation::SYNC_FILE);
        }
        return inner_.SyncFile(file_id);
    }

    [[nodiscard]] DiskResult<PageNo> PageCount(FileId file_id) override {
        return inner_.PageCount(file_id);
    }

    [[nodiscard]] DiskResult<PageNo> ExtendFile(FileId file_id) override {
        ++extends_;
        if (TakeFlag(fail_next_extend_)) {
            const auto status = Injected(PageId{.file_id = file_id, .page_no = INVALID_PAGE_NO},
                                         DiskOperation::EXTEND_FILE);
            return {.success = false, .value = 0, .error = status.error};
        }
        return inner_.ExtendFile(file_id);
    }

  private:
    [[nodiscard]] bool TakeFlag(bool& flag) {
        const std::scoped_lock lock{mutex_};
        return std::exchange(flag, false);
    }

    [[nodiscard]] static DiskStatus Injected(PageId page_id, DiskOperation operation) {
        return {.success = false,
                .error = DiskError{.code = DiskErrorCode::SYSTEM_ERROR,
                                   .operation = operation,
                                   .file_id = page_id.file_id,
                                   .page_no = page_id.page_no,
                                   .path = {},
                                   .system_error = EIO,
                                   .message = "injected fault"}};
    }

    DiskPageIo inner_;
    std::mutex mutex_;
    bool fail_next_read_{false};
    WriteFault next_write_fault_{WriteFault::NONE};
    bool fail_next_sync_{false};
    bool fail_next_extend_{false};
    std::atomic<std::size_t> reads_{0};
    std::atomic<std::size_t> writes_{0};
    std::atomic<std::size_t> syncs_{0};
    std::atomic<std::size_t> extends_{0};
};

// Pauses the first thread that reaches a chosen event (optionally for one page) until Resume().
// All other events pass through. Optionally runs a callback on every event.
class PausingObserver final : public BufferPoolObserver {
  public:
    void PauseAt(BufferPoolEventType type, std::optional<PageId> page_id = std::nullopt) {
        const std::scoped_lock lock{mutex_};
        pause_type_ = type;
        pause_page_ = page_id;
        armed_ = true;
        paused_ = false;
        resumed_ = false;
    }

    void WaitUntilPaused() {
        std::unique_lock lock{mutex_};
        changed_.wait(lock, [&] { return paused_; });
    }

    void Resume() {
        const std::scoped_lock lock{mutex_};
        resumed_ = true;
        changed_.notify_all();
    }

    void SetCallback(std::function<void(const BufferPoolEvent&)> callback) {
        const std::scoped_lock lock{mutex_};
        callback_ = std::move(callback);
    }

    [[nodiscard]] std::size_t Count(BufferPoolEventType type) const {
        const std::scoped_lock lock{mutex_};
        return static_cast<std::size_t>(std::ranges::count(events_, type));
    }

    void OnEvent(const BufferPoolEvent& event) override {
        std::function<void(const BufferPoolEvent&)> callback;
        std::unique_lock lock{mutex_};
        events_.push_back(event.type);
        callback = callback_;
        const bool matches = armed_ && event.type == pause_type_ &&
                             (!pause_page_.has_value() || *pause_page_ == event.page_id);
        if (matches) {
            armed_ = false;
            paused_ = true;
            changed_.notify_all();
            changed_.wait(lock, [&] { return resumed_; });
        }
        lock.unlock();
        if (callback) {
            callback(event);
        }
    }

  private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    BufferPoolEventType pause_type_{BufferPoolEventType::LOAD_INTENT_INSTALLED};
    std::optional<PageId> pause_page_;
    bool armed_{false};
    bool paused_{false};
    bool resumed_{false};
    std::function<void(const BufferPoolEvent&)> callback_;
    std::vector<BufferPoolEventType> events_;
};

inline PageValidationOutcome ValidateHeap(Page& page) {
    return HeapPage{page}.Validate() ? PageValidationOutcome::VALID
                                     : PageValidationOutcome::CORRUPT;
}

inline bool InitializeHeap(Page& page) {
    return HeapPage{page}.Initialize();
}

// Spin (yielding) until a predicate over observable pool state holds. State-based, not
// time-based: there is no sleep and no elapsed-time oracle.
template <typename Predicate>
void WaitFor(Predicate&& predicate) {
    while (!predicate()) {
        std::this_thread::yield();
    }
}

inline constexpr FileId HEAP_FILE_ID = 7;

// One registered heap file with a DiskManager, PageFile, and fault-injecting I/O.
class HeapFileFixture {
  public:
    HeapFileFixture() : io_(disk_) {}

    [[nodiscard]] bool Create(FileId file_id = HEAP_FILE_ID) {
        if (!directory_.valid()) {
            return false;
        }
        path_ = directory_.File("heap.dat");
        auto created = PageFile::Create(disk_,
                                        path_,
                                        FileSuperblock{.file_kind = FileKind::HEAP,
                                                       .file_id = file_id,
                                                       .flags = 0,
                                                       .page_lsn = INVALID_LSN,
                                                       .object_id = 1,
                                                       .creation_epoch = 1});
        if (!created.page_file.has_value()) {
            return false;
        }
        page_file_.emplace(std::move(*created.page_file));
        file_id_ = file_id;
        return true;
    }

    // Appends a heap page directly through the raw layer (as if written by an earlier process),
    // with a valid checksum. `mutate` may damage the image before it is written.
    [[nodiscard]] std::optional<PageId>
    AppendRawHeapPage(const std::function<void(Page&)>& mutate = {}) {
        if (!page_file_.has_value()) {
            return std::nullopt;
        }
        const auto allocated = page_file_->AllocatePage();
        if (!allocated.page_id.has_value()) {
            return std::nullopt;
        }
        Page page{*allocated.page_id};
        if (!HeapPage{page}.Initialize()) {
            return std::nullopt;
        }
        FinalizePageChecksum(page.Bytes());
        if (mutate) {
            mutate(page);
        }
        if (!disk_.WritePage(page.Id(), page.Bytes())) {
            return std::nullopt;
        }
        return page.Id();
    }

    [[nodiscard]] bool OverwriteRawPage(const Page& page) {
        return static_cast<bool>(disk_.WritePage(page.Id(), page.Bytes()));
    }

    // Reads the on-disk image, bypassing the pool. Records a failure if the read fails.
    [[nodiscard]] Page ReadRawPage(PageId page_id) {
        Page page{page_id};
        if (!disk_.ReadPage(page_id, page.Bytes())) {
            ADD_FAILURE() << "raw read of page " << page_id.page_no << " failed";
        }
        return page;
    }

    [[nodiscard]] DiskManager& disk() noexcept {
        return disk_;
    }
    [[nodiscard]] FaultInjectingPageIo& io() noexcept {
        return io_;
    }
    [[nodiscard]] FileId file_id() const noexcept {
        return file_id_;
    }
    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }
    void ClosePageFile() {
        page_file_.reset();
    }

  private:
    TemporaryDirectory directory_;
    DiskManager disk_;
    FaultInjectingPageIo io_;
    std::optional<PageFile> page_file_;
    std::filesystem::path path_;
    FileId file_id_{INVALID_FILE_ID};
};

// Non-optional accessors so tests do not dereference unchecked optionals.
[[nodiscard]] inline PageTableSnapshot EntryOf(const BufferPool& pool, PageId page_id) {
    auto entry = pool.InspectPage(page_id);
    if (!entry.has_value()) {
        ADD_FAILURE() << "page " << page_id.page_no << " has no page-table entry";
        return {};
    }
    return *entry;
}

[[nodiscard]] inline FrameSnapshot FrameOf(const BufferPool& pool, PageId page_id) {
    const auto entry = EntryOf(pool, page_id);
    if (!entry.frame.has_value()) {
        ADD_FAILURE() << "page " << page_id.page_no << " has no bound frame";
        return {};
    }
    return *entry.frame;
}

// First byte of a heap tuple, or nullopt-equivalent 0xFF sentinel with a recorded failure.
[[nodiscard]] inline std::byte FirstTupleByte(const Page& page, SlotId slot = 0) {
    const auto tuple = HeapPage{const_cast<Page&>(page)}.TupleBytes(slot);
    if (!tuple.has_value() || tuple->empty()) {
        ADD_FAILURE() << "slot " << slot << " holds no tuple";
        return std::byte{0xFF};
    }
    return tuple->front();
}

// Unwraps a successful fetch; records a failure and returns an inert guard otherwise.
template <typename Guard>
[[nodiscard]] Guard Must(BufferResult<Guard>&& result) {
    if (!result.value.has_value()) {
        ADD_FAILURE() << "BufferPool operation failed: " << result.error.message;
        return Guard{};
    }
    return std::move(*result.value);
}

[[nodiscard]] inline PageId Pid(PageNo page_no) {
    return PageId{.file_id = HEAP_FILE_ID, .page_no = page_no};
}

} // namespace dblusblus::buffer_test

#endif // DBLUSBLUS_TESTS_STORAGE_BUFFER_TEST_SUPPORT_H_
