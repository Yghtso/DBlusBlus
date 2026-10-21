#include "storage/buffer/buffer_pool.h"

#include "storage/page/page_checksum.h"
#include "storage/page/page_header.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <shared_mutex>
#include <string>
#include <utility>

namespace dblusblus {
namespace {

using PageImage = std::array<std::byte, PAGE_SIZE>;

[[nodiscard]] BufferError MakeError(BufferErrorCode code,
                                    BufferOperation operation,
                                    PageId page_id,
                                    std::string message,
                                    std::optional<DiskError> disk_error = std::nullopt) {
    return {.code = code,
            .operation = operation,
            .page_id = page_id,
            .disk_error = std::move(disk_error),
            .message = std::move(message)};
}

template <typename Value>
[[nodiscard]] BufferResult<Value> Failure(const BufferError& error) {
    return {.value = std::nullopt, .error = error};
}

[[nodiscard]] BufferStatus FailureStatus(BufferError error) {
    return {.error = std::move(error)};
}

[[nodiscard]] BufferErrorCode ReadErrorCode(const DiskError& error) noexcept {
    return error.code == DiskErrorCode::PAGE_NOT_FOUND ? BufferErrorCode::FILE_OR_PAGE_NOT_FOUND
                                                       : BufferErrorCode::RAW_IO_FAILURE;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Construction

BufferPool::BufferPool(DiskManager& disk_manager, BufferPoolOptions options)
    : BufferPool(nullptr, std::make_unique<DiskPageIo>(disk_manager), options) {}

BufferPool::BufferPool(PageIo& page_io, BufferPoolOptions options)
    : BufferPool(&page_io, nullptr, options) {}

BufferPool::BufferPool(PageIo* page_io, std::unique_ptr<PageIo> owned_io, BufferPoolOptions options)
    : owned_io_(std::move(owned_io)), io_(page_io != nullptr ? page_io : owned_io_.get()),
      frame_count_(std::max<std::size_t>(options.frame_count, 1U)),
      max_pin_count_(std::max<std::uint32_t>(options.max_pin_count, 1U)),
      observer_(options.observer), frames_(frame_count_), replacer_(frame_count_) {
    assert(options.frame_count > 0U);
    free_list_.reserve(frame_count_);
    // Pop from the back, so frame 0 is handed out first.
    for (std::size_t index = frame_count_; index > 0U; --index) {
        free_list_.push_back(static_cast<FrameId>(index - 1U));
    }
}

BufferPool::~BufferPool() {
#ifndef NDEBUG
    const std::scoped_lock lock{mutex_};
    for (std::size_t index = 0; index < frame_count_; ++index) {
        assert(frames_[index].pin_count == 0U && "BufferPool destroyed with outstanding pins");
    }
#endif
}

// ---------------------------------------------------------------------------------------------
// File registry

BufferStatus BufferPool::RegisterFile(FileId file_id, PageValidator validator) {
    const PageId file_page{.file_id = file_id, .page_no = INVALID_PAGE_NO};
    if (file_id == INVALID_FILE_ID) {
        return FailureStatus(MakeError(BufferErrorCode::FILE_OR_PAGE_NOT_FOUND,
                                       BufferOperation::REGISTER_FILE,
                                       file_page,
                                       "invalid FileId"));
    }
    if (!validator) {
        return FailureStatus(MakeError(BufferErrorCode::INTERNAL_INVARIANT_FAILURE,
                                       BufferOperation::REGISTER_FILE,
                                       file_page,
                                       "an owner page validator is required"));
    }
    const auto page_count = io_->PageCount(file_id);
    if (!page_count) {
        return FailureStatus(MakeError(ReadErrorCode(page_count.error),
                                       BufferOperation::REGISTER_FILE,
                                       file_page,
                                       "cannot determine the file's page count",
                                       page_count.error));
    }

    const std::scoped_lock lock{mutex_};
    if (quiescing_) {
        return FailureStatus(MakeError(BufferErrorCode::BUFFERPOOL_QUIESCING,
                                       BufferOperation::REGISTER_FILE,
                                       file_page,
                                       "BufferPool is quiescing"));
    }
    if (files_.contains(file_id)) {
        return FailureStatus(MakeError(BufferErrorCode::INTERNAL_INVARIANT_FAILURE,
                                       BufferOperation::REGISTER_FILE,
                                       file_page,
                                       "FileId is already registered with this BufferPool"));
    }
    files_.emplace(file_id,
                   FileEntry{.state = FileState::ACTIVE,
                             .validator = std::move(validator),
                             .published_page_count = page_count.value,
                             .new_page_in_progress = false});
    return {};
}

BufferStatus BufferPool::RetireFile(FileId file_id, RetireDisposition disposition) {
    const PageId file_page{.file_id = file_id, .page_no = INVALID_PAGE_NO};
    std::unique_lock lock{mutex_};
    auto file = files_.find(file_id);
    if (file == files_.end()) {
        return FailureStatus(MakeError(BufferErrorCode::FILE_OR_PAGE_NOT_FOUND,
                                       BufferOperation::RETIRE_FILE,
                                       file_page,
                                       "FileId is not registered with this BufferPool"));
    }

    // Retirement-gate linearization point: no new intents or pins after this.
    if (file->second.state == FileState::ACTIVE) {
        file->second.state = FileState::RETIRING;
        state_changed_.notify_all();
        Notify(lock,
               {.type = BufferPoolEventType::FILE_RETIRING,
                .page_id = file_page,
                .frame_id = std::nullopt});
    }

    // Drain existing guards, loads, evictions, writebacks, and new-page construction.
    state_changed_.wait(lock, [&] {
        if (files_.at(file_id).new_page_in_progress) {
            return false;
        }
        return std::ranges::all_of(page_table_, [&](const auto& item) {
            const auto& [page_id, entry] = item;
            if (page_id.file_id != file_id) {
                return true;
            }
            if (entry.state != PageTableEntryState::RESIDENT) {
                return false;
            }
            const BufferFrame& frame = frames_[entry.frame_id];
            return frame.pin_count == 0U && frame.io_state == FrameIoState::NONE;
        });
    });

    if (disposition == RetireDisposition::FLUSH_DIRTY) {
        if (auto status = FlushMatching(lock, file_id, BufferOperation::RETIRE_FILE); !status) {
            return status;
        }
    }

    // Remove mappings, reset frames, then close the registration.
    for (auto entry = page_table_.begin(); entry != page_table_.end();) {
        if (entry->first.file_id != file_id) {
            ++entry;
            continue;
        }
        const FrameId frame_id = entry->second.frame_id;
        entry = page_table_.erase(entry);
        ResetFrame(frame_id);
        ReleaseFrameToFreeList(frame_id);
    }
    files_.erase(file_id);
    state_changed_.notify_all();
    Notify(
        lock,
        {.type = BufferPoolEventType::FILE_CLOSED, .page_id = file_page, .frame_id = std::nullopt});
    return {};
}

// ---------------------------------------------------------------------------------------------
// Fetch

BufferResult<ReadPageGuard> BufferPool::FetchRead(PageId page_id) {
    auto pinned = PinPage(page_id);
    if (!pinned.value.has_value()) {
        return Failure<ReadPageGuard>(pinned.error);
    }
    return MakeGuard<ReadPageGuard>(*pinned.value, LatchMode::SHARED);
}

BufferResult<WritePageGuard> BufferPool::FetchWrite(PageId page_id) {
    auto pinned = PinPage(page_id);
    if (!pinned.value.has_value()) {
        return Failure<WritePageGuard>(pinned.error);
    }
    return MakeGuard<WritePageGuard>(*pinned.value, LatchMode::EXCLUSIVE);
}

template <typename Guard>
BufferResult<Guard> BufferPool::MakeGuard(FrameId frame_id, LatchMode mode) {
    BufferFrame& frame = frames_[frame_id];
    // Pin first, then latch (§7.7). The pin keeps the binding stable while we wait.
    if (mode == LatchMode::SHARED) {
        frame.latch.lock_shared();
    } else {
        frame.latch.lock();
    }
    // binding_epoch only changes while pin_count == 0, so the pinned value is stable.
    return {.value = Guard{*this, frame_id, frame.page, frame.binding_epoch}, .error = {}};
}

BufferResult<FrameId> BufferPool::PinPage(PageId page_id) {
    std::unique_lock lock{mutex_};
    bool hit_notified = false;
    for (;;) {
        if (quiescing_) {
            return Failure<FrameId>(MakeError(BufferErrorCode::BUFFERPOOL_QUIESCING,
                                              BufferOperation::FETCH_PAGE,
                                              page_id,
                                              "BufferPool is quiescing"));
        }
        const auto file = files_.find(page_id.file_id);
        if (file == files_.end()) {
            return Failure<FrameId>(MakeError(BufferErrorCode::FILE_OR_PAGE_NOT_FOUND,
                                              BufferOperation::FETCH_PAGE,
                                              page_id,
                                              "FileId is not registered with this BufferPool"));
        }
        if (file->second.state != FileState::ACTIVE) {
            return Failure<FrameId>(MakeError(BufferErrorCode::FILE_RETIRED_OR_CLOSING,
                                              BufferOperation::FETCH_PAGE,
                                              page_id,
                                              "file is retiring"));
        }
        // Page 0 is the superblock and is served only by the PageFile open path. Pages at or
        // beyond the published bound are not ordinary page addresses.
        if (page_id.page_no == 0U || page_id.page_no >= file->second.published_page_count) {
            return Failure<FrameId>(MakeError(BufferErrorCode::FILE_OR_PAGE_NOT_FOUND,
                                              BufferOperation::FETCH_PAGE,
                                              page_id,
                                              "page is not a published data page"));
        }

        const auto entry = page_table_.find(page_id);
        if (entry == page_table_.end()) {
            return LoadPage(lock, page_id);
        }

        switch (entry->second.state) {
        case PageTableEntryState::RESIDENT: {
            if (observer_ != nullptr && !hit_notified) {
                hit_notified = true;
                Notify(lock,
                       {.type = BufferPoolEventType::HIT_PIN_ATTEMPT,
                        .page_id = page_id,
                        .frame_id = entry->second.frame_id});
                continue; // Re-examine: eviction may have won while the lock was released.
            }
            const FrameId frame_id = entry->second.frame_id;
            BufferFrame& frame = frames_[frame_id];
            if (frame.pin_count >= max_pin_count_) {
                return Failure<FrameId>(MakeError(BufferErrorCode::PIN_COUNT_OVERFLOW,
                                                  BufferOperation::FETCH_PAGE,
                                                  page_id,
                                                  "pin count is at its maximum"));
            }
            // Resident-hit linearization point.
            ++frame.pin_count;
            replacer_.SetReferenced(frame_id);
            return {.value = frame_id, .error = {}};
        }
        case PageTableEntryState::EVICTING:
            // The old frame cannot be pinned. Wait for eviction to finish or fail, then retry.
            state_changed_.wait(lock);
            continue;
        case PageTableEntryState::LOAD_INTENT:
        case PageTableEntryState::LOADING: {
            const std::shared_ptr<LoadOperation> load = entry->second.load;
            if (load == nullptr) {
                // A private new-page construction; its PageNo is not yet published.
                return Failure<FrameId>(MakeError(BufferErrorCode::FILE_OR_PAGE_NOT_FOUND,
                                                  BufferOperation::FETCH_PAGE,
                                                  page_id,
                                                  "page is not a published data page"));
            }
            if (load->claims >= max_pin_count_) {
                return Failure<FrameId>(MakeError(BufferErrorCode::PIN_COUNT_OVERFLOW,
                                                  BufferOperation::FETCH_PAGE,
                                                  page_id,
                                                  "too many pending fetch claims"));
            }
            ++load->claims;
            state_changed_.wait(lock, [&] { return load->done; });
            if (load->frame_id.has_value()) {
                // Publication already converted this claim into one pin.
                return {.value = load->frame_id, .error = {}};
            }
            return Failure<FrameId>(load->error);
        }
        }
    }
}

BufferResult<FrameId> BufferPool::LoadPage(std::unique_lock<std::mutex>& lock, PageId page_id) {
    auto load = std::make_shared<LoadOperation>();
    load->claims = 1U; // the loader's own claim
    page_table_.emplace(
        page_id,
        PageTableEntry{.state = PageTableEntryState::LOAD_INTENT, .frame_id = 0, .load = load});
    Notify(lock,
           {.type = BufferPoolEventType::LOAD_INTENT_INSTALLED,
            .page_id = page_id,
            .frame_id = std::nullopt});

    const auto fail = [&](BufferError error) {
        error.operation = BufferOperation::FETCH_PAGE;
        page_table_.erase(page_id);
        load->error = error;
        load->done = true;
        state_changed_.notify_all();
        Notify(lock,
               {.type = BufferPoolEventType::LOAD_FAILED,
                .page_id = page_id,
                .frame_id = std::nullopt});
        return Failure<FrameId>(error);
    };

    auto acquired = AcquireFrame(lock, page_id, BufferOperation::FETCH_PAGE);
    if (!acquired.value.has_value()) {
        return fail(std::move(acquired.error));
    }
    const FrameId frame_id = *acquired.value;
    BufferFrame& frame = frames_[frame_id];

    // FREE -> LOADING + READ_IN_PROGRESS, bound to the sole intent before the read starts.
    ++frame.binding_epoch;
    frame.page = Page{page_id};
    frame.state = FrameState::LOADING;
    frame.io_state = FrameIoState::READ_IN_PROGRESS;
    frame.dirty = false;
    PageTableEntry& entry = page_table_.at(page_id);
    entry.state = PageTableEntryState::LOADING;
    entry.frame_id = frame_id;
    // FileEntry references are stable; the file cannot close while this load is in progress.
    const PageValidator& validator = files_.at(page_id.file_id).validator;

    lock.unlock();
    NotifyUnlocked({.type = BufferPoolEventType::FRAME_BOUND_LOADING,
                    .page_id = page_id,
                    .frame_id = frame_id});
    const DiskStatus read = io_->ReadPage(page_id, frame.page.Bytes());
    const PageValidationOutcome outcome =
        read ? ValidateLoadedPage(frame, validator, true) : PageValidationOutcome::VALID;
    lock.lock();

    std::optional<BufferError> error;
    if (!read) {
        error = MakeError(ReadErrorCode(read.error),
                          BufferOperation::FETCH_PAGE,
                          page_id,
                          "page read failed",
                          read.error);
    } else if (outcome == PageValidationOutcome::CORRUPT) {
        error = MakeError(BufferErrorCode::CORRUPT_PAGE,
                          BufferOperation::FETCH_PAGE,
                          page_id,
                          "page failed validation");
    } else if (outcome == PageValidationOutcome::UNSUPPORTED_FORMAT) {
        error = MakeError(BufferErrorCode::UNSUPPORTED_PAGE_FORMAT,
                          BufferOperation::FETCH_PAGE,
                          page_id,
                          "page uses an unsupported format");
    } else if (files_.at(page_id.file_id).state != FileState::ACTIVE) {
        error = MakeError(BufferErrorCode::FILE_RETIRED_OR_CLOSING,
                          BufferOperation::FETCH_PAGE,
                          page_id,
                          "file began retiring during load");
    }
    if (error.has_value()) {
        // LOADING -> FREE: nothing was published; all joiners receive this error.
        ResetFrame(frame_id);
        ReleaseFrameToFreeList(frame_id);
        return fail(std::move(*error));
    }

    // LOADING -> RESIDENT: one atomic publication that turns every claim into one pin.
    frame.state = FrameState::RESIDENT;
    frame.io_state = FrameIoState::NONE;
    frame.pin_count = load->claims;
    replacer_.SetReferenced(frame_id);
    PageTableEntry& published = page_table_.at(page_id);
    published.state = PageTableEntryState::RESIDENT;
    published.load.reset();
    load->frame_id = frame_id;
    load->done = true;
    state_changed_.notify_all();
    Notify(lock,
           {.type = BufferPoolEventType::LOAD_PUBLISHED, .page_id = page_id, .frame_id = frame_id});
    return {.value = frame_id, .error = {}};
}

PageValidationOutcome BufferPool::ValidateLoadedPage(BufferFrame& frame,
                                                     const PageValidator& validator,
                                                     bool verify_checksum) {
    const auto bytes = std::as_const(frame.page).Bytes();
    if (verify_checksum && !VerifyPageChecksum(bytes)) {
        return PageValidationOutcome::CORRUPT;
    }
    const auto header = DecodeCommonPageHeader(bytes);
    // Identity only: page type, version, and header geometry are the owner validator's domain.
    if (!header.has_value() || header->page_no != frame.page.Id().page_no) {
        return PageValidationOutcome::CORRUPT;
    }
    return validator(frame.page);
}

// ---------------------------------------------------------------------------------------------
// Replacement

bool BufferPool::IsVictimEligible(FrameId frame_id) const {
    const BufferFrame& frame = frames_[frame_id];
    if (frame.state != FrameState::RESIDENT || frame.pin_count != 0U ||
        frame.io_state != FrameIoState::NONE) {
        return false;
    }
    // Latch holders and waiters always hold pins, so pin_count == 0 also excludes them.
    // Frames of a retiring file belong to its drain, not to replacement.
    const auto file = files_.find(frame.page.Id().file_id);
    return file != files_.end() && file->second.state == FileState::ACTIVE;
}

BufferResult<FrameId> BufferPool::AcquireFrame(std::unique_lock<std::mutex>& lock,
                                               PageId requester,
                                               BufferOperation operation) {
    bool candidate_notified = false;
    for (;;) {
        if (!free_list_.empty()) {
            const FrameId frame_id = free_list_.back();
            free_list_.pop_back();
            return {.value = frame_id, .error = {}};
        }

        const auto candidate = replacer_.FindCandidate(
            [this](FrameId frame_id) { return IsVictimEligible(frame_id); });
        if (!candidate.has_value()) {
            return Failure<FrameId>(MakeError(BufferErrorCode::NO_REPLACEABLE_FRAME,
                                              operation,
                                              requester,
                                              "no frame is free or replaceable"));
        }
        const FrameId frame_id = *candidate;
        BufferFrame& frame = frames_[frame_id];
        const PageId victim = frame.page.Id();

        if (observer_ != nullptr && !candidate_notified) {
            candidate_notified = true;
            Notify(lock,
                   {.type = BufferPoolEventType::VICTIM_CANDIDATE_OBSERVED,
                    .page_id = victim,
                    .frame_id = frame_id});
            // Final recheck after the lock was released. Losing the race is not an error.
            if (!IsVictimEligible(frame_id) || frame.page.Id() != victim) {
                continue;
            }
            if (!free_list_.empty()) {
                continue;
            }
        }

        // Victim reservation: RESIDENT -> EVICTING. Old mapping stays, but becomes non-pinnable.
        frame.state = FrameState::EVICTING;
        page_table_.at(victim).state = PageTableEntryState::EVICTING;
        Notify(lock,
               {.type = BufferPoolEventType::VICTIM_RESERVED,
                .page_id = victim,
                .frame_id = frame_id});

        if (frame.dirty) {
            // EVICTING + WRITEBACK_IN_PROGRESS. No pins or latch users exist and no new access is
            // possible, so the image is stable without the page latch.
            frame.io_state = FrameIoState::WRITEBACK_IN_PROGRESS;
            PageImage image{};
            std::ranges::copy(std::as_const(frame.page).Bytes(), image.begin());
            lock.unlock();
            NotifyUnlocked({.type = BufferPoolEventType::WRITEBACK_IMAGE_COPIED,
                            .page_id = victim,
                            .frame_id = frame_id});
            BufferStatus written = WriteStableImage(victim, frame_id, image, operation);
            lock.lock();
            frame.io_state = FrameIoState::NONE;
            if (!written) {
                // Restore the old page as pinnable RESIDENT with its dirty state intact. The
                // requesting operation fails with this error; no other victim hides it.
                frame.state = FrameState::RESIDENT;
                page_table_.at(victim).state = PageTableEntryState::RESIDENT;
                state_changed_.notify_all();
                written.error.message = "eviction writeback failed: " + written.error.message;
                return Failure<FrameId>(written.error);
            }
        }

        // Remove the old mapping before resetting identity; the frame passes directly to the
        // requester without entering the free list.
        page_table_.erase(victim);
        ResetFrame(frame_id);
        state_changed_.notify_all();
        return {.value = frame_id, .error = {}};
    }
}

void BufferPool::ResetFrame(FrameId frame_id) {
    BufferFrame& frame = frames_[frame_id];
    frame.page = Page{};
#ifndef NDEBUG
    // Make stale borrowed views into a reset frame recognizable.
    std::ranges::fill(frame.page.Bytes(), std::byte{0xDB});
#endif
    frame.state = FrameState::FREE;
    frame.io_state = FrameIoState::NONE;
    frame.pin_count = 0U;
    frame.dirty = false;
    // modification_generation and binding_epoch are intentionally not reset (§7.12.2).
    replacer_.ClearReferenced(frame_id);
}

void BufferPool::ReleaseFrameToFreeList(FrameId frame_id) {
    assert(frames_[frame_id].state == FrameState::FREE);
    free_list_.push_back(frame_id);
}

// ---------------------------------------------------------------------------------------------
// New pages

BufferResult<WritePageGuard> BufferPool::NewPage(FileId file_id,
                                                 const PageInitializer& initializer) {
    PageId page_id{.file_id = file_id, .page_no = INVALID_PAGE_NO};
    std::unique_lock lock{mutex_};
    for (;;) {
        if (quiescing_) {
            return Failure<WritePageGuard>(MakeError(BufferErrorCode::BUFFERPOOL_QUIESCING,
                                                     BufferOperation::NEW_PAGE,
                                                     page_id,
                                                     "BufferPool is quiescing"));
        }
        const auto file = files_.find(file_id);
        if (file == files_.end()) {
            return Failure<WritePageGuard>(
                MakeError(BufferErrorCode::FILE_OR_PAGE_NOT_FOUND,
                          BufferOperation::NEW_PAGE,
                          page_id,
                          "FileId is not registered with this BufferPool"));
        }
        if (file->second.state != FileState::ACTIVE) {
            return Failure<WritePageGuard>(MakeError(BufferErrorCode::FILE_RETIRED_OR_CLOSING,
                                                     BufferOperation::NEW_PAGE,
                                                     page_id,
                                                     "file is retiring"));
        }
        // Append publication is serialized per file so the published bound stays contiguous.
        if (!file->second.new_page_in_progress) {
            break;
        }
        state_changed_.wait(lock);
    }
    FileEntry& file = files_.at(file_id);
    file.new_page_in_progress = true;
    const auto finish = [&] {
        file.new_page_in_progress = false;
        state_changed_.notify_all();
    };

    // Reserve the frame before extending, so a full pool never grows the file.
    auto acquired = AcquireFrame(lock, page_id, BufferOperation::NEW_PAGE);
    if (!acquired.value.has_value()) {
        finish();
        acquired.error.operation = BufferOperation::NEW_PAGE;
        return Failure<WritePageGuard>(acquired.error);
    }
    const FrameId frame_id = *acquired.value;
    BufferFrame& frame = frames_[frame_id];

    // A physical page beyond the published bound was left by an earlier failed creation. It was
    // never published, so its PageNo is reused instead of growing the file again.
    const PageNo published = file.published_page_count;
    lock.unlock();
    std::optional<DiskError> io_error;
    const auto physical = io_->PageCount(file_id);
    if (!physical) {
        io_error = physical.error;
    } else if (physical.value > published) {
        page_id.page_no = published;
    } else {
        const auto extended = io_->ExtendFile(file_id);
        if (extended) {
            page_id.page_no = extended.value;
        } else {
            io_error = extended.error;
        }
    }
    lock.lock();
    if (io_error.has_value()) {
        ReleaseFrameToFreeList(frame_id);
        finish();
        return Failure<WritePageGuard>(MakeError(BufferErrorCode::RAW_IO_FAILURE,
                                                 BufferOperation::NEW_PAGE,
                                                 page_id,
                                                 "file extension failed",
                                                 std::move(io_error)));
    }
    if (page_id.page_no == 0U || page_id.page_no != file.published_page_count) {
        ReleaseFrameToFreeList(frame_id);
        finish();
        return Failure<WritePageGuard>(
            MakeError(BufferErrorCode::INTERNAL_INVARIANT_FAILURE,
                      BufferOperation::NEW_PAGE,
                      page_id,
                      "extension did not return the next page after the published bound"));
    }

    // FREE -> LOADING + NONE: private, unpublished, constructed without reading disk bytes.
    ++frame.binding_epoch;
    frame.page = Page{page_id};
    frame.state = FrameState::LOADING;
    frame.io_state = FrameIoState::NONE;
    frame.dirty = false;
    page_table_.emplace(page_id,
                        PageTableEntry{.state = PageTableEntryState::LOADING,
                                       .frame_id = frame_id,
                                       .load = nullptr});
    const PageValidator& validator = file.validator;

    lock.unlock();
    const bool built = initializer && initializer(frame.page);
    const PageValidationOutcome outcome =
        built ? ValidateLoadedPage(frame, validator, false) : PageValidationOutcome::CORRUPT;
    lock.lock();

    if (!built || outcome != PageValidationOutcome::VALID || file.state != FileState::ACTIVE) {
        const bool retiring = file.state != FileState::ACTIVE;
        page_table_.erase(page_id);
        ResetFrame(frame_id);
        ReleaseFrameToFreeList(frame_id);
        finish();
        // Without WAL or a truncation primitive the appended page stays at the file tail, outside
        // the published bound; the next NewPage reuses its PageNo (§4.11.1.1 restoration is
        // approximated by reuse, not truncation).
        return Failure<WritePageGuard>(
            MakeError(retiring ? BufferErrorCode::FILE_RETIRED_OR_CLOSING
                               : BufferErrorCode::INTERNAL_INVARIANT_FAILURE,
                      BufferOperation::NEW_PAGE,
                      page_id,
                      retiring ? "file began retiring during page creation"
                               : "page initializer did not produce a valid page"));
    }

    // Co-publish the advanced bound and the RESIDENT frame with the creator's pin.
    file.published_page_count = page_id.page_no + 1U;
    frame.state = FrameState::RESIDENT;
    frame.pin_count = 1U;
    frame.dirty = true;
    ++frame.modification_generation;
    replacer_.SetReferenced(frame_id);
    page_table_.at(page_id).state = PageTableEntryState::RESIDENT;
    finish();
    lock.unlock();
    return MakeGuard<WritePageGuard>(frame_id, LatchMode::EXCLUSIVE);
}

// ---------------------------------------------------------------------------------------------
// Flushing

BufferStatus BufferPool::WriteStableImage(PageId page_id,
                                          std::optional<FrameId> frame_id,
                                          std::span<std::byte, PAGE_SIZE> image,
                                          BufferOperation operation) {
    // §4.12.2: the checksum describes exactly the private image being written.
    FinalizePageChecksum(image);
    // WAL-before-data (§7.11) has no work to do yet: there is no WAL and pages carry INVALID_LSN.
    NotifyUnlocked({.type = BufferPoolEventType::WRITEBACK_WRITE_STARTED,
                    .page_id = page_id,
                    .frame_id = frame_id});
    const DiskStatus written = io_->WritePage(page_id, image);
    if (!written) {
        return FailureStatus(MakeError(BufferErrorCode::RAW_IO_FAILURE,
                                       operation,
                                       page_id,
                                       "page write failed",
                                       written.error));
    }
    NotifyUnlocked({.type = BufferPoolEventType::WRITEBACK_SYNC_STARTED,
                    .page_id = page_id,
                    .frame_id = frame_id});
    // A pwrite alone is not stable; fdatasync must succeed before the frame may become clean.
    const DiskStatus synced = io_->SyncFile(page_id.file_id);
    if (!synced) {
        return FailureStatus(MakeError(BufferErrorCode::RAW_IO_FAILURE,
                                       operation,
                                       page_id,
                                       "page file synchronization failed",
                                       synced.error));
    }
    NotifyUnlocked({.type = BufferPoolEventType::WRITEBACK_RECONCILE_STARTED,
                    .page_id = page_id,
                    .frame_id = frame_id});
    return {};
}

BufferStatus BufferPool::WritebackResident(std::unique_lock<std::mutex>& lock,
                                           FrameId frame_id,
                                           BufferOperation operation) {
    BufferFrame& frame = frames_[frame_id];
    assert(frame.state == FrameState::RESIDENT &&
           frame.io_state == FrameIoState::WRITEBACK_IN_PROGRESS);
    const PageId page_id = frame.page.Id();
    const std::uint64_t binding_epoch = frame.binding_epoch;

    lock.unlock();
    PageImage image{};
    std::uint64_t copied_generation = 0;
    {
        // Copy a stable image under the read latch. Writers publish generations while holding
        // the exclusive latch, so the generation read here matches the copied bytes.
        const std::shared_lock latch{frame.latch};
        std::ranges::copy(std::as_const(frame.page).Bytes(), image.begin());
        copied_generation = frame.modification_generation;
    }
    NotifyUnlocked({.type = BufferPoolEventType::WRITEBACK_IMAGE_COPIED,
                    .page_id = page_id,
                    .frame_id = frame_id});
    BufferStatus written = WriteStableImage(page_id, frame_id, image, operation);
    lock.lock();

    frame.io_state = FrameIoState::NONE;
    // Clean only if this residency still holds exactly the copied generation.
    if (written && frame.page.Id() == page_id && frame.binding_epoch == binding_epoch &&
        frame.modification_generation == copied_generation) {
        frame.dirty = false;
    }
    state_changed_.notify_all();
    return written;
}

BufferStatus BufferPool::FlushPageLocked(std::unique_lock<std::mutex>& lock,
                                         PageId page_id,
                                         BufferOperation operation) {
    for (;;) {
        const auto entry = page_table_.find(page_id);
        if (entry == page_table_.end()) {
            return {};
        }
        switch (entry->second.state) {
        case PageTableEntryState::LOAD_INTENT:
        case PageTableEntryState::LOADING:
            // Nothing published; there is no resident generation to make durable.
            return {};
        case PageTableEntryState::EVICTING:
            state_changed_.wait(lock);
            continue;
        case PageTableEntryState::RESIDENT:
            break;
        }
        const FrameId frame_id = entry->second.frame_id;
        BufferFrame& frame = frames_[frame_id];
        if (!frame.dirty) {
            return {};
        }
        if (frame.io_state != FrameIoState::NONE) {
            // Join the active writeback, then recheck the current generation.
            state_changed_.wait(lock);
            continue;
        }
        frame.io_state = FrameIoState::WRITEBACK_IN_PROGRESS;
        if (auto status = WritebackResident(lock, frame_id, operation); !status) {
            return status;
        }
    }
}

BufferStatus BufferPool::FlushMatching(std::unique_lock<std::mutex>& lock,
                                       std::optional<FileId> file_id,
                                       BufferOperation operation) {
    std::vector<PageId> dirty_pages;
    for (const auto& [page_id, entry] : page_table_) {
        if (file_id.has_value() && page_id.file_id != *file_id) {
            continue;
        }
        if (entry.state == PageTableEntryState::RESIDENT && frames_[entry.frame_id].dirty) {
            dirty_pages.push_back(page_id);
        }
    }
    std::ranges::sort(dirty_pages);
    BufferStatus first_failure;
    for (const PageId page_id : dirty_pages) {
        auto status = FlushPageLocked(lock, page_id, operation);
        if (!status && first_failure.Ok()) {
            first_failure = std::move(status);
        }
    }
    return first_failure;
}

BufferStatus BufferPool::FlushPage(PageId page_id) {
    std::unique_lock lock{mutex_};
    return FlushPageLocked(lock, page_id, BufferOperation::FLUSH_PAGE);
}

BufferStatus BufferPool::FlushFile(FileId file_id) {
    std::unique_lock lock{mutex_};
    if (!files_.contains(file_id)) {
        return FailureStatus(MakeError(BufferErrorCode::FILE_OR_PAGE_NOT_FOUND,
                                       BufferOperation::FLUSH_PAGE,
                                       PageId{.file_id = file_id, .page_no = INVALID_PAGE_NO},
                                       "FileId is not registered with this BufferPool"));
    }
    return FlushMatching(lock, file_id, BufferOperation::FLUSH_PAGE);
}

BufferStatus BufferPool::FlushAll() {
    std::unique_lock lock{mutex_};
    return FlushMatching(lock, std::nullopt, BufferOperation::FLUSH_PAGE);
}

BufferStatus BufferPool::Shutdown() {
    std::unique_lock lock{mutex_};
    quiescing_ = true;
    state_changed_.notify_all();
    state_changed_.wait(lock, [&] {
        const bool frames_idle = std::ranges::all_of(frames_, [](const BufferFrame& frame) {
            return frame.pin_count == 0U && frame.io_state == FrameIoState::NONE;
        });
        const bool table_idle = std::ranges::all_of(page_table_, [](const auto& item) {
            return item.second.state == PageTableEntryState::RESIDENT;
        });
        const bool files_idle = std::ranges::none_of(
            files_, [](const auto& item) { return item.second.new_page_in_progress; });
        return frames_idle && table_idle && files_idle;
    });
    return FlushMatching(lock, std::nullopt, BufferOperation::SHUTDOWN);
}

// ---------------------------------------------------------------------------------------------
// Guard support

void BufferPool::Unpin(FrameId frame_id) noexcept {
    const std::scoped_lock lock{mutex_};
    BufferFrame& frame = frames_[frame_id];
    assert(frame.pin_count > 0U && "unpin without a pin");
    if (frame.pin_count == 0U) {
        return;
    }
    --frame.pin_count;
    if (frame.pin_count == 0U) {
        state_changed_.notify_all();
    }
}

void BufferPool::PublishMutation(FrameId frame_id) noexcept {
    const std::scoped_lock lock{mutex_};
    BufferFrame& frame = frames_[frame_id];
    assert(frame.state == FrameState::RESIDENT && frame.pin_count > 0U);
    // A 64-bit generation advanced once per mutation cannot wrap in practice; treat it as an
    // invariant rather than silently repeating a token.
    assert(frame.modification_generation != std::numeric_limits<std::uint64_t>::max());
    ++frame.modification_generation;
    frame.dirty = true;
}

void BufferPool::Notify(std::unique_lock<std::mutex>& lock, const BufferPoolEvent& event) {
    if (observer_ == nullptr) {
        return;
    }
    lock.unlock();
    observer_->OnEvent(event);
    lock.lock();
}

void BufferPool::NotifyUnlocked(const BufferPoolEvent& event) {
    if (observer_ != nullptr) {
        observer_->OnEvent(event);
    }
}

// ---------------------------------------------------------------------------------------------
// Diagnostics

FrameSnapshot BufferPool::SnapshotFrameLocked(FrameId frame_id) const {
    const BufferFrame& frame = frames_[frame_id];
    return {.frame_id = frame_id,
            .page_id = frame.page.Id(),
            .state = frame.state,
            .io_state = frame.io_state,
            .pin_count = frame.pin_count,
            .dirty = frame.dirty,
            .referenced = replacer_.IsReferenced(frame_id),
            .modification_generation = frame.modification_generation,
            .binding_epoch = frame.binding_epoch};
}

std::size_t BufferPool::FreeFrameCount() const {
    const std::scoped_lock lock{mutex_};
    return free_list_.size();
}

FrameSnapshot BufferPool::InspectFrame(FrameId frame_id) const {
    const std::scoped_lock lock{mutex_};
    return SnapshotFrameLocked(frame_id);
}

std::optional<PageTableSnapshot> BufferPool::InspectPage(PageId page_id) const {
    const std::scoped_lock lock{mutex_};
    const auto entry = page_table_.find(page_id);
    if (entry == page_table_.end()) {
        return std::nullopt;
    }
    PageTableSnapshot snapshot{.state = entry->second.state,
                               .frame = std::nullopt,
                               .pending_claims =
                                   entry->second.load ? entry->second.load->claims : 0U};
    if (entry->second.state != PageTableEntryState::LOAD_INTENT) {
        snapshot.frame = SnapshotFrameLocked(entry->second.frame_id);
    }
    return snapshot;
}

std::size_t BufferPool::LastClockPassVisits() const {
    const std::scoped_lock lock{mutex_};
    return replacer_.LastPassVisits();
}

bool BufferPool::IsCurrentBinding(FrameId frame_id,
                                  PageId page_id,
                                  std::uint64_t binding_epoch) const {
    const std::scoped_lock lock{mutex_};
    const BufferFrame& frame = frames_[frame_id];
    return frame.state != FrameState::FREE && frame.page.Id() == page_id &&
           frame.binding_epoch == binding_epoch;
}

} // namespace dblusblus
