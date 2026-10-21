#include "storage/buffer/page_guard.h"

#include "common/assert.h"
#include "storage/buffer/buffer_pool.h"
#include "storage/page/page_checksum.h"

#include <utility>

namespace dblusblus {

// ---------------------------------------------------------------------------------------------
// PageGuardBase

namespace detail {

PageGuardBase::PageGuardBase(BufferPool& pool,
                             FrameId frame_id,
                             Page& page,
                             std::uint64_t binding_epoch) noexcept
    : pool_(&pool), page_(&page), frame_id_(frame_id), binding_epoch_(binding_epoch) {}

PageGuardBase::PageGuardBase(PageGuardBase&& other) noexcept {
    TakeFrom(other);
}

void PageGuardBase::TakeFrom(PageGuardBase& other) noexcept {
    pool_ = std::exchange(other.pool_, nullptr);
    page_ = std::exchange(other.page_, nullptr);
    frame_id_ = other.frame_id_;
    binding_epoch_ = other.binding_epoch_;
}

PageId PageGuardBase::Id() const noexcept {
    DBLUS_ASSERT(Valid(), "page guard used after release");
    return page_->Id();
}

const Page& PageGuardBase::GetPage() const noexcept {
    DBLUS_ASSERT(Valid(), "page guard used after release");
    return *page_;
}

std::span<const std::byte, PAGE_SIZE> PageGuardBase::Bytes() const noexcept {
    DBLUS_ASSERT(Valid(), "page guard used after release");
    return std::as_const(*page_).Bytes();
}

void PageGuardBase::ReleaseLatchAndPin(Latch latch) noexcept {
    if (pool_ == nullptr) {
        return;
    }
    BufferPool* pool = std::exchange(pool_, nullptr);
    const PageId page_id = std::exchange(page_, nullptr)->Id();
    // Latch before pin: the pin keeps the frame bound until the latch is gone.
    auto& frame_latch = pool->frames_[frame_id_].latch;
    if (latch == Latch::SHARED) {
        frame_latch.unlock_shared();
    } else {
        frame_latch.unlock();
    }
    pool->NotifyUnlocked({.type = BufferPoolEventType::GUARD_LATCH_RELEASED,
                          .page_id = page_id,
                          .frame_id = frame_id_});
    pool->Unpin(frame_id_);
}

} // namespace detail

// ---------------------------------------------------------------------------------------------
// ReadPageGuard

ReadPageGuard::~ReadPageGuard() {
    Release();
}

ReadPageGuard& ReadPageGuard::operator=(ReadPageGuard&& other) noexcept {
    if (this != &other) {
        Release();
        TakeFrom(other);
    }
    return *this;
}

void ReadPageGuard::Release() noexcept {
    ReleaseLatchAndPin(Latch::SHARED);
}

// ---------------------------------------------------------------------------------------------
// WritePageGuard

WritePageGuard::~WritePageGuard() {
    Release();
}

WritePageGuard::WritePageGuard(WritePageGuard&& other) noexcept {
    TakeFrom(other);
#ifndef NDEBUG
    mutable_accessed_ = std::exchange(other.mutable_accessed_, false);
    published_checksum_ = other.published_checksum_;
#endif
}

WritePageGuard& WritePageGuard::operator=(WritePageGuard&& other) noexcept {
    if (this != &other) {
        Release();
        TakeFrom(other);
#ifndef NDEBUG
        mutable_accessed_ = std::exchange(other.mutable_accessed_, false);
        published_checksum_ = other.published_checksum_;
#endif
    }
    return *this;
}

Page& WritePageGuard::MutablePage() noexcept {
    DBLUS_ASSERT(Valid(), "page guard used after release");
#ifndef NDEBUG
    if (!mutable_accessed_) {
        mutable_accessed_ = true;
        published_checksum_ = ComputePageChecksum(Bytes());
    }
#endif
    return MutablePageUnchecked();
}

std::span<std::byte, PAGE_SIZE> WritePageGuard::MutableBytes() noexcept {
    return MutablePage().Bytes();
}

void WritePageGuard::MarkDirty() noexcept {
    DBLUS_ASSERT(Valid(), "page guard used after release");
    Pool().PublishMutation(Frame());
#ifndef NDEBUG
    mutable_accessed_ = true;
    published_checksum_ = ComputePageChecksum(Bytes());
#endif
}

void WritePageGuard::Release() noexcept {
    if (!Valid()) {
        return;
    }
#ifndef NDEBUG
    // ARCHITECTURE.md §7.10.1: an unreported persistent mutation is an invariant violation.
    DBLUS_CHECK(!mutable_accessed_ || ComputePageChecksum(Bytes()) == published_checksum_,
                "write guard released with page changes that MarkDirty() did not publish");
    mutable_accessed_ = false;
#endif
    ReleaseLatchAndPin(Latch::EXCLUSIVE);
}

} // namespace dblusblus
