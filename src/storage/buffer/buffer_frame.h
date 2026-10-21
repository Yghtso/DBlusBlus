#pragma once

#include "common/types.h"
#include "storage/buffer/clock_replacer.h"
#include "storage/page/page.h"

#include <cstdint>
#include <shared_mutex>

namespace dblusblus {

// Canonical frame ownership state (ARCHITECTURE.md §7.6.1).
enum class FrameState : std::uint8_t {
    FREE,
    LOADING,
    RESIDENT,
    EVICTING,
};

// Orthogonal I/O substate. Legal combinations:
//   FREE      NONE
//   LOADING   READ_IN_PROGRESS (existing page) or NONE (new page under construction)
//   RESIDENT  NONE or WRITEBACK_IN_PROGRESS (copied writeback; page stays usable)
//   EVICTING  NONE or WRITEBACK_IN_PROGRESS (eviction writeback; no ordinary access)
enum class FrameIoState : std::uint8_t {
    NONE,
    READ_IN_PROGRESS,
    WRITEBACK_IN_PROGRESS,
};

// One buffer frame. All metadata fields are protected by the BufferPool mutex; page bytes are
// protected by `latch` while the frame is RESIDENT and by exclusive ownership of the LOADING or
// EVICTING reservation otherwise. Frame metadata is process-local and never persisted.
//
// Not included until the WAL exists: rec_lsn, DPT membership, checkpoint/FPI epoch metadata, and
// MTR no-flush state (ARCHITECTURE.md §§12.10, 12.16).
struct BufferFrame {
    // Owns the 8192 page bytes and the bound PageId. Page::Id() is INVALID while FREE.
    Page page;
    FrameState state{FrameState::FREE};
    FrameIoState io_state{FrameIoState::NONE};
    std::uint32_t pin_count{0};
    bool dirty{false};
    // Incremented once per published persistent mutation. Monotonic for the frame's lifetime and
    // never reset on reassignment, so a stale writeback completion cannot match a later residency.
    std::uint64_t modification_generation{0};
    // Incremented each time the frame binds a PageId. Together with the PageId this identifies one
    // residency; used to reject stale completions and to detect stale borrowed views in tests.
    std::uint64_t binding_epoch{0};
    // Page latch: shared for ReadPageGuard and writeback image copies, exclusive for
    // WritePageGuard. Lock order: a page latch may be acquired before the pool mutex, never after.
    std::shared_mutex latch;
};

} // namespace dblusblus
