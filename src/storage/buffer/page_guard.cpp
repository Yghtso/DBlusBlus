#include "storage/buffer/page_guard.h"

#include "common/assert.h"
#include "storage/buffer/buffer_pool.h"
#include "storage/page/page_checksum.h"

#include <cstdlib>
#include <utility>

namespace dblusblus {

// ---------------------------------------------------------------------------------------------
// ReadPageGuard

ReadPageGuard::ReadPageGuard(BufferPool& pool,
                             FrameId frame_id,
                             Page& page,
                             std::uint64_t binding_epoch) noexcept
    : pool_(&pool), page_(&page), frame_id_(frame_id), binding_epoch_(binding_epoch) {}

ReadPageGuard::~ReadPageGuard() {
    Release();
}

ReadPageGuard::ReadPageGuard(ReadPageGuard&& other) noexcept
    : pool_(std::exchange(other.pool_, nullptr)), page_(std::exchange(other.page_, nullptr)),
      frame_id_(other.frame_id_), binding_epoch_(other.binding_epoch_) {}

ReadPageGuard& ReadPageGuard::operator=(ReadPageGuard&& other) noexcept {
    if (this != &other) {
        Release();
        pool_ = std::exchange(other.pool_, nullptr);
        page_ = std::exchange(other.page_, nullptr);
        frame_id_ = other.frame_id_;
        binding_epoch_ = other.binding_epoch_;
    }
    return *this;
}

PageId ReadPageGuard::Id() const noexcept {
    DBLUS_ASSERT(Valid(), "page guard used after release");
    return page_->Id();
}

const Page& ReadPageGuard::GetPage() const noexcept {
    DBLUS_ASSERT(Valid(), "page guard used after release");
    return *page_;
}

std::span<const std::byte, PAGE_SIZE> ReadPageGuard::Bytes() const noexcept {
    DBLUS_ASSERT(Valid(), "page guard used after release");
    return std::as_const(*page_).Bytes();
}

void ReadPageGuard::Release() noexcept {
    if (pool_ == nullptr) {
        return;
    }
    BufferPool* pool = std::exchange(pool_, nullptr);
    const PageId page_id = page_->Id();
    page_ = nullptr;
    // Latch before pin: the pin keeps the frame bound until the latch is gone.
    pool->frames_[frame_id_].latch.unlock_shared();
    pool->NotifyUnlocked({.type = BufferPoolEventType::GUARD_LATCH_RELEASED,
                          .page_id = page_id,
                          .frame_id = frame_id_});
    pool->Unpin(frame_id_);
}

// ---------------------------------------------------------------------------------------------
// WritePageGuard

WritePageGuard::WritePageGuard(BufferPool& pool,
                               FrameId frame_id,
                               Page& page,
                               std::uint64_t binding_epoch) noexcept
    : pool_(&pool), page_(&page), frame_id_(frame_id), binding_epoch_(binding_epoch) {}

WritePageGuard::~WritePageGuard() {
    Release();
}

void WritePageGuard::MoveFrom(WritePageGuard& other) noexcept {
    pool_ = std::exchange(other.pool_, nullptr);
    page_ = std::exchange(other.page_, nullptr);
    frame_id_ = other.frame_id_;
    binding_epoch_ = other.binding_epoch_;
#ifndef NDEBUG
    mutable_accessed_ = std::exchange(other.mutable_accessed_, false);
    published_checksum_ = other.published_checksum_;
#endif
}

WritePageGuard::WritePageGuard(WritePageGuard&& other) noexcept {
    MoveFrom(other);
}

WritePageGuard& WritePageGuard::operator=(WritePageGuard&& other) noexcept {
    if (this != &other) {
        Release();
        MoveFrom(other);
    }
    return *this;
}

PageId WritePageGuard::Id() const noexcept {
    DBLUS_ASSERT(Valid(), "page guard used after release");
    return page_->Id();
}

const Page& WritePageGuard::GetPage() const noexcept {
    DBLUS_ASSERT(Valid(), "page guard used after release");
    return *page_;
}

std::span<const std::byte, PAGE_SIZE> WritePageGuard::Bytes() const noexcept {
    DBLUS_ASSERT(Valid(), "page guard used after release");
    return std::as_const(*page_).Bytes();
}

Page& WritePageGuard::MutablePage() noexcept {
    DBLUS_ASSERT(Valid(), "page guard used after release");
#ifndef NDEBUG
    if (!mutable_accessed_) {
        mutable_accessed_ = true;
        published_checksum_ = ComputePageChecksum(std::as_const(*page_).Bytes());
    }
#endif
    return *page_;
}

std::span<std::byte, PAGE_SIZE> WritePageGuard::MutableBytes() noexcept {
    return MutablePage().Bytes();
}

void WritePageGuard::MarkDirty() noexcept {
    DBLUS_ASSERT(Valid(), "page guard used after release");
    pool_->PublishMutation(frame_id_);
#ifndef NDEBUG
    mutable_accessed_ = true;
    published_checksum_ = ComputePageChecksum(std::as_const(*page_).Bytes());
#endif
}

void WritePageGuard::Release() noexcept {
    if (pool_ == nullptr) {
        return;
    }
#ifndef NDEBUG
    // ARCHITECTURE.md §7.10.1: an unreported persistent mutation is an invariant violation.
    if (mutable_accessed_ &&
        ComputePageChecksum(std::as_const(*page_).Bytes()) != published_checksum_) {
        std::abort();
    }
    mutable_accessed_ = false;
#endif
    BufferPool* pool = std::exchange(pool_, nullptr);
    const PageId page_id = page_->Id();
    page_ = nullptr;
    pool->frames_[frame_id_].latch.unlock();
    pool->NotifyUnlocked({.type = BufferPoolEventType::GUARD_LATCH_RELEASED,
                          .page_id = page_id,
                          .frame_id = frame_id_});
    pool->Unpin(frame_id_);
}

} // namespace dblusblus
