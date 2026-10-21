#ifndef DBLUSBLUS_STORAGE_BUFFER_PAGE_GUARD_H_
#define DBLUSBLUS_STORAGE_BUFFER_PAGE_GUARD_H_

#include "common/types.h"
#include "storage/buffer/clock_replacer.h"
#include "storage/page/page.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace dblusblus {

class BufferPool;

// RAII ownership of one pin plus one shared page latch (ARCHITECTURE.md §7.7).
//
// The guard is move-only. Moving transfers the pin and latch and leaves the source inert.
// Release() (or destruction) releases the latch first and the pin second, exactly once.
//
// Every pointer, span, or page-controller view obtained through the guard becomes invalid when
// the guard releases, even if other callers still pin the frame.
class ReadPageGuard {
  public:
    ReadPageGuard() noexcept = default;
    ~ReadPageGuard();

    ReadPageGuard(const ReadPageGuard&) = delete;
    ReadPageGuard& operator=(const ReadPageGuard&) = delete;
    ReadPageGuard(ReadPageGuard&& other) noexcept;
    ReadPageGuard& operator=(ReadPageGuard&& other) noexcept;

    [[nodiscard]] bool Valid() const noexcept {
        return pool_ != nullptr;
    }
    [[nodiscard]] PageId Id() const noexcept;
    [[nodiscard]] const Page& GetPage() const noexcept;
    [[nodiscard]] std::span<const std::byte, PAGE_SIZE> Bytes() const noexcept;

    // Diagnostic identity of the residency this guard protects.
    [[nodiscard]] FrameId Frame() const noexcept {
        return frame_id_;
    }
    [[nodiscard]] std::uint64_t BindingEpoch() const noexcept {
        return binding_epoch_;
    }

    void Release() noexcept;

  private:
    friend class BufferPool;
    ReadPageGuard(BufferPool& pool,
                  FrameId frame_id,
                  Page& page,
                  std::uint64_t binding_epoch) noexcept;

    BufferPool* pool_{nullptr};
    Page* page_{nullptr};
    FrameId frame_id_{0};
    std::uint64_t binding_epoch_{0};
};

// RAII ownership of one pin plus one exclusive page latch (ARCHITECTURE.md §7.7, §7.10.1).
//
// Acquiring a write guard does not make the page dirty. A caller that changes persistent page
// bytes must call MarkDirty() before releasing the guard; MarkDirty() publishes the mutation by
// incrementing the frame's modification generation and setting the dirty flag. In debug builds,
// releasing a guard whose bytes differ from what the latest MarkDirty() published (or, without one,
// from the bytes at the first mutable access) is an invariant violation and aborts.
class WritePageGuard {
  public:
    WritePageGuard() noexcept = default;
    ~WritePageGuard();

    WritePageGuard(const WritePageGuard&) = delete;
    WritePageGuard& operator=(const WritePageGuard&) = delete;
    WritePageGuard(WritePageGuard&& other) noexcept;
    WritePageGuard& operator=(WritePageGuard&& other) noexcept;

    [[nodiscard]] bool Valid() const noexcept {
        return pool_ != nullptr;
    }
    [[nodiscard]] PageId Id() const noexcept;
    [[nodiscard]] const Page& GetPage() const noexcept;
    [[nodiscard]] std::span<const std::byte, PAGE_SIZE> Bytes() const noexcept;

    [[nodiscard]] Page& MutablePage() noexcept;
    [[nodiscard]] std::span<std::byte, PAGE_SIZE> MutableBytes() noexcept;

    // Publishes the persistent mutation(s) made through this guard so far.
    void MarkDirty() noexcept;

    [[nodiscard]] FrameId Frame() const noexcept {
        return frame_id_;
    }
    [[nodiscard]] std::uint64_t BindingEpoch() const noexcept {
        return binding_epoch_;
    }

    void Release() noexcept;

  private:
    friend class BufferPool;
    WritePageGuard(BufferPool& pool,
                   FrameId frame_id,
                   Page& page,
                   std::uint64_t binding_epoch) noexcept;

    void MoveFrom(WritePageGuard& other) noexcept;

    BufferPool* pool_{nullptr};
    Page* page_{nullptr};
    FrameId frame_id_{0};
    std::uint64_t binding_epoch_{0};
#ifndef NDEBUG
    // Debug-only detection of unreported mutation: checksum of the bytes as of the first mutable
    // access or the latest MarkDirty().
    bool mutable_accessed_{false};
    std::uint32_t published_checksum_{0};
#endif
};

} // namespace dblusblus

#endif // DBLUSBLUS_STORAGE_BUFFER_PAGE_GUARD_H_
