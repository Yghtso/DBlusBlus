#include "storage/heap/fsm_candidate_index.h"

#include "common/assert.h"
#include "storage/heap/fsm_page.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace dblusblus {
namespace {

[[nodiscard]] bool IsValidHeapPageNumber(PageNo page_no) noexcept {
    return page_no != 0 && page_no != INVALID_PAGE_NO;
}

} // namespace

Status<FsmCandidateIndexError> FsmCandidateIndex::Upsert(PageNo page_no, std::uint8_t category) {
    if (!IsValidHeapPageNumber(page_no)) {
        return Unexpected{FsmCandidateIndexError::INVALID_PAGE_NUMBER};
    }

    const auto existing = page_categories_.find(page_no);
    if (existing == page_categories_.end()) {
        const auto [category_iterator, inserted_in_map] =
            page_categories_.emplace(page_no, category);
        DBLUS_ASSERT(inserted_in_map, "page was absent from the category map");
        try {
            [[maybe_unused]] const auto [bucket_iterator, inserted_in_bucket] =
                buckets_[category].insert(page_no);
            DBLUS_ASSERT(inserted_in_bucket, "page was absent from its category bucket");
        } catch (...) {
            page_categories_.erase(category_iterator);
            throw;
        }
        return {};
    }
    if (existing->second == category) {
        DBLUS_ASSERT(buckets_[category].contains(page_no), "map and buckets agree");
        return {};
    }

    [[maybe_unused]] const auto [bucket_iterator, inserted_in_new_bucket] =
        buckets_[category].insert(page_no);
    DBLUS_ASSERT(inserted_in_new_bucket, "page was absent from its new category bucket");
    const std::size_t erased = buckets_[existing->second].erase(page_no);
    DBLUS_ASSERT(erased == 1U, "page was in its old category bucket");
    existing->second = category;
    return {};
}

Status<FsmCandidateIndexError> FsmCandidateIndex::Remove(PageNo page_no) {
    if (!IsValidHeapPageNumber(page_no)) {
        return Unexpected{FsmCandidateIndexError::INVALID_PAGE_NUMBER};
    }

    const auto existing = page_categories_.find(page_no);
    if (existing == page_categories_.end()) {
        return {};
    }

    const std::size_t erased = buckets_[existing->second].erase(page_no);
    DBLUS_ASSERT(erased == 1U, "page was in its category bucket");
    page_categories_.erase(existing);
    return {};
}

FsmCandidateResult
FsmCandidateIndex::FindCandidate(std::size_t required_tuple_bytes) const noexcept {
    const auto minimum_category = MinimumFsmCategoryForTupleBytes(required_tuple_bytes);
    if (!minimum_category.Ok()) {
        DBLUS_ASSERT(minimum_category.Error() == FsmTupleRequestError::TUPLE_TOO_LARGE,
                     "TUPLE_TOO_LARGE is the only tuple-request error");
        return Unexpected{FsmCandidateIndexError::TUPLE_REQUEST_TOO_LARGE};
    }

    const std::size_t first_category = *minimum_category;
    constexpr std::size_t maximum_category = std::numeric_limits<std::uint8_t>::max();
    for (std::size_t category = first_category; category <= maximum_category; ++category) {
        if (!buckets_[category].empty()) {
            return std::optional<PageNo>{*buckets_[category].begin()};
        }
    }

    return std::optional<PageNo>{};
}

void FsmCandidateIndex::Clear() noexcept {
    for (auto& bucket : buckets_) {
        bucket.clear();
    }
    page_categories_.clear();
}

std::size_t FsmCandidateIndex::Size() const noexcept {
    return page_categories_.size();
}

} // namespace dblusblus
