#pragma once

#include "common/types.h"
#include "storage/buffer/clock_replacer.h"
#include "storage/page/page.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace dblusblus {

class BufferPool;

namespace detail {

// State and read access shared by ReadPageGuard and WritePageGuard: the pool, the page, and the
// residency (frame + binding epoch) the guard pins. Not used on its own.
class PageGuardBase {
  public:
    PageGuardBase(const PageGuardBase&) = delete;
    PageGuardBase& operator=(const PageGuardBase&) = delete;
    // Each guard type releases its own latch before taking another's, so it defines the move
    // assignment itself.
    PageGuardBase& operator=(PageGuardBase&&) = delete;

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

  protected:
    enum class Latch : std::uint8_t {
        SHARED,
        EXCLUSIVE,
    };

    PageGuardBase() noexcept = default;
    PageGuardBase(BufferPool& pool,
                  FrameId frame_id,
                  Page& page,
                  std::uint64_t binding_epoch) noexcept;
    PageGuardBase(PageGuardBase&& other) noexcept;
    ~PageGuardBase() = default;

    // Takes other's pin and latch, leaving other inert. The caller has already released its own.
    void TakeFrom(PageGuardBase& other) noexcept;
    // Releases the latch first and the pin second, then leaves this guard inert.
    void ReleaseLatchAndPin(Latch latch) noexcept;

    [[nodiscard]] BufferPool& Pool() const noexcept {
        return *pool_;
    }
    [[nodiscard]] Page& MutablePageUnchecked() const noexcept {
        return *page_;
    }

  private:
    BufferPool* pool_{nullptr};
    Page* page_{nullptr};
    FrameId frame_id_{0};
    std::uint64_t binding_epoch_{0};
};

} // namespace detail

// RAII ownership of one pin plus one shared page latch (ARCHITECTURE.md §7.7).
//
// The guard is move-only. Moving transfers the pin and latch and leaves the source inert.
// Release() (or destruction) releases the latch first and the pin second, exactly once.
//
// Every pointer, span, or page-controller view obtained through the guard becomes invalid when
// the guard releases, even if other callers still pin the frame.
class ReadPageGuard : public detail::PageGuardBase {
  public:
    ReadPageGuard() noexcept = default;
    ~ReadPageGuard();

    ReadPageGuard(const ReadPageGuard&) = delete;
    ReadPageGuard& operator=(const ReadPageGuard&) = delete;
    ReadPageGuard(ReadPageGuard&& other) noexcept = default;
    ReadPageGuard& operator=(ReadPageGuard&& other) noexcept;

    void Release() noexcept;

  private:
    friend class BufferPool;
    using PageGuardBase::PageGuardBase;
};

// RAII ownership of one pin plus one exclusive page latch (ARCHITECTURE.md §7.7, §7.10.1).
//
// Acquiring a write guard does not make the page dirty. A caller that changes persistent page
// bytes must call MarkDirty() before releasing the guard; MarkDirty() publishes the mutation by
// incrementing the frame's modification generation and setting the dirty flag. In debug builds,
// releasing a guard whose bytes differ from what the latest MarkDirty() published (or, without one,
// from the bytes at the first mutable access) is an invariant violation and aborts.
class WritePageGuard : public detail::PageGuardBase {
  public:
    WritePageGuard() noexcept = default;
    ~WritePageGuard();

    WritePageGuard(const WritePageGuard&) = delete;
    WritePageGuard& operator=(const WritePageGuard&) = delete;
    WritePageGuard(WritePageGuard&& other) noexcept;
    WritePageGuard& operator=(WritePageGuard&& other) noexcept;

    [[nodiscard]] Page& MutablePage() noexcept;
    [[nodiscard]] std::span<std::byte, PAGE_SIZE> MutableBytes() noexcept;

    // Publishes the persistent mutation(s) made through this guard so far.
    void MarkDirty() noexcept;

    void Release() noexcept;

  private:
    friend class BufferPool;
    using PageGuardBase::PageGuardBase;

#ifndef NDEBUG
    // Debug-only detection of unreported mutation: checksum of the bytes as of the first mutable
    // access or the latest MarkDirty().
    bool mutable_accessed_{false};
    std::uint32_t published_checksum_{0};
#endif
};

} // namespace dblusblus
