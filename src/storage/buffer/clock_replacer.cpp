#include "storage/buffer/clock_replacer.h"

#include <cassert>

namespace dblusblus {

ClockReplacer::ClockReplacer(std::size_t frame_count) : referenced_(frame_count, 0U) {
    assert(frame_count > 0U);
}

void ClockReplacer::SetReferenced(FrameId frame_id) noexcept {
    referenced_[frame_id] = 1U;
}

void ClockReplacer::ClearReferenced(FrameId frame_id) noexcept {
    referenced_[frame_id] = 0U;
}

bool ClockReplacer::IsReferenced(FrameId frame_id) const noexcept {
    return referenced_[frame_id] != 0U;
}

} // namespace dblusblus
