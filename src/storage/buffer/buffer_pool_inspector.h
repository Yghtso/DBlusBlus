#pragma once

#include "common/types.h"
#include "storage/buffer/buffer_frame.h"
#include "storage/buffer/buffer_pool.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace dblusblus::storage {

// Diagnostic snapshot of one frame, taken atomically under the pool mutex.
struct FrameSnapshot {
    FrameId frame_id{0};
    PageId page_id{};
    FrameState state{FrameState::FREE};
    FrameIoState io_state{FrameIoState::NONE};
    std::uint32_t pin_count{0};
    bool dirty{false};
    bool referenced{false};
    std::uint64_t modification_generation{0};
    std::uint64_t binding_epoch{0};
};

// Diagnostic snapshot of one page-table entry.
struct PageTableSnapshot {
    PageTableEntryState state{PageTableEntryState::LOAD_INTENT};
    std::optional<FrameSnapshot> frame;
    std::uint32_t pending_claims{0};
};

// Read-only view of a BufferPool's frames, page table and CLOCK state, for tests and
// diagnostics. Each call takes the pool mutex, so a snapshot is consistent but may be stale by
// the time the caller reads it. Production code does not use this class.
class BufferPoolInspector {
  public:
    explicit BufferPoolInspector(const BufferPool& pool) noexcept : pool_(&pool) {}

    [[nodiscard]] FrameSnapshot InspectFrame(FrameId frame_id) const;
    [[nodiscard]] std::optional<PageTableSnapshot> InspectPage(PageId page_id) const;
    [[nodiscard]] std::size_t LastClockPassVisits() const;
    // True while the guard's residency (frame + PageId + binding epoch) is still the frame's
    // current binding. Used to detect stale views.
    [[nodiscard]] bool
    IsCurrentBinding(FrameId frame_id, PageId page_id, std::uint64_t binding_epoch) const;

  private:
    // Requires the pool mutex.
    [[nodiscard]] FrameSnapshot SnapshotFrameLocked(FrameId frame_id) const;

    const BufferPool* pool_;
};

} // namespace dblusblus::storage
