#ifndef DBLUSBLUS_STORAGE_BUFFER_CLOCK_REPLACER_H_
#define DBLUSBLUS_STORAGE_BUFFER_CLOCK_REPLACER_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace dblusblus {

using FrameId = std::uint32_t;

// CLOCK replacement policy (ARCHITECTURE.md §7.12). It owns only the clock hand and one reference
// bit per frame; BufferPool supplies the complete §7.9 eligibility predicate and performs the
// atomic victim reservation itself. Not thread-safe: BufferPool calls it under its own mutex.
class ClockReplacer {
  public:
    explicit ClockReplacer(std::size_t frame_count);

    void SetReferenced(FrameId frame_id) noexcept;
    void ClearReferenced(FrameId frame_id) noexcept;
    [[nodiscard]] bool IsReferenced(FrameId frame_id) const noexcept;

    // One complete CLOCK attempt. Starting at the hand, each frame is visited at most twice:
    //   ineligible  -> skip
    //   referenced  -> clear the reference bit, skip
    //   otherwise   -> candidate
    // Two rotations are enough for every eligible frame whose bit was cleared on the first
    // rotation to be reached again. Returns nullopt when no eligible frame exists. The hand
    // advances past a returned candidate.
    template <typename Eligible>
    [[nodiscard]] std::optional<FrameId> FindCandidate(Eligible&& eligible) {
        last_pass_visits_ = 0;
        const std::size_t frame_count = referenced_.size();
        for (std::size_t step = 0; step < 2U * frame_count; ++step) {
            const auto frame_id = static_cast<FrameId>(hand_);
            hand_ = (hand_ + 1U) % frame_count;
            ++last_pass_visits_;
            if (!eligible(frame_id)) {
                continue;
            }
            if (referenced_[frame_id] != 0U) {
                referenced_[frame_id] = 0U;
                continue;
            }
            return frame_id;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::size_t Hand() const noexcept {
        return hand_;
    }
    // Frames visited by the most recent FindCandidate call; used to verify the one-pass bound.
    [[nodiscard]] std::size_t LastPassVisits() const noexcept {
        return last_pass_visits_;
    }

  private:
    std::vector<std::uint8_t> referenced_;
    std::size_t hand_{0};
    std::size_t last_pass_visits_{0};
};

} // namespace dblusblus

#endif // DBLUSBLUS_STORAGE_BUFFER_CLOCK_REPLACER_H_
