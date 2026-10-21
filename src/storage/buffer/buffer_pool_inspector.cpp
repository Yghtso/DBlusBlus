#include "storage/buffer/buffer_pool_inspector.h"

#include <mutex>

namespace dblusblus::storage {

FrameSnapshot BufferPoolInspector::SnapshotFrameLocked(FrameId frame_id) const {
    const BufferFrame& frame = pool_->frames_[frame_id];
    return {.frame_id = frame_id,
            .page_id = frame.page.Id(),
            .state = frame.state,
            .io_state = frame.io_state,
            .pin_count = frame.pin_count,
            .dirty = frame.dirty,
            .referenced = pool_->replacer_.IsReferenced(frame_id),
            .modification_generation = frame.modification_generation,
            .binding_epoch = frame.binding_epoch};
}

FrameSnapshot BufferPoolInspector::InspectFrame(FrameId frame_id) const {
    const std::scoped_lock lock{pool_->mutex_};
    return SnapshotFrameLocked(frame_id);
}

std::optional<PageTableSnapshot> BufferPoolInspector::InspectPage(PageId page_id) const {
    const std::scoped_lock lock{pool_->mutex_};
    const auto entry = pool_->page_table_.find(page_id);
    if (entry == pool_->page_table_.end()) {
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

std::size_t BufferPoolInspector::LastClockPassVisits() const {
    const std::scoped_lock lock{pool_->mutex_};
    return pool_->replacer_.LastPassVisits();
}

bool BufferPoolInspector::IsCurrentBinding(FrameId frame_id,
                                           PageId page_id,
                                           std::uint64_t binding_epoch) const {
    const std::scoped_lock lock{pool_->mutex_};
    const BufferFrame& frame = pool_->frames_[frame_id];
    return frame.state != FrameState::FREE && frame.page.Id() == page_id &&
           frame.binding_epoch == binding_epoch;
}

} // namespace dblusblus::storage
