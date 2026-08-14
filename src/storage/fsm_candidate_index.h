#ifndef DBLUSBLUS_STORAGE_FSM_CANDIDATE_INDEX_H_
#define DBLUSBLUS_STORAGE_FSM_CANDIDATE_INDEX_H_

#include "common/types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <unordered_map>

namespace dblusblus {

enum class FsmCandidateIndexError : std::uint8_t {
    NONE,
    INVALID_PAGE_NUMBER,
    TUPLE_REQUEST_TOO_LARGE,
};

struct FsmCandidateResult {
    std::optional<PageNo> page_no;
    FsmCandidateIndexError error{FsmCandidateIndexError::NONE};
};

// Relation-local, rebuildable FSM metadata. This class performs no I/O and is not thread-safe.
class FsmCandidateIndex {
  public:
    // Replaces any existing category for page_no. Page 0 and INVALID_PAGE_NO are rejected.
    [[nodiscard]] FsmCandidateIndexError Upsert(PageNo page_no, std::uint8_t category);
    // Removing an untracked valid page is a successful no-op.
    [[nodiscard]] FsmCandidateIndexError Remove(PageNo page_no);
    // A successful result with no page means that no known category can satisfy the request.
    [[nodiscard]] FsmCandidateResult FindCandidate(std::size_t required_tuple_bytes) const noexcept;

    void Clear() noexcept;

    [[nodiscard]] std::size_t Size() const noexcept;

  private:
    static constexpr std::size_t CATEGORY_COUNT = 256;

    std::array<std::set<PageNo>, CATEGORY_COUNT> buckets_;
    std::unordered_map<PageNo, std::uint8_t> page_categories_;
};

} // namespace dblusblus

#endif // DBLUSBLUS_STORAGE_FSM_CANDIDATE_INDEX_H_
