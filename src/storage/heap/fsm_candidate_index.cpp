#include "storage/heap/fsm_candidate_index.h"

#include "storage/heap/fsm_page.h"

#include <cassert>
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

FsmCandidateIndexError FsmCandidateIndex::Upsert(PageNo page_no, std::uint8_t category) {
    if (!IsValidHeapPageNumber(page_no)) {
        return FsmCandidateIndexError::INVALID_PAGE_NUMBER;
    }

    const auto existing = page_categories_.find(page_no);
    if (existing == page_categories_.end()) {
        const auto [category_iterator, inserted_in_map] =
            page_categories_.emplace(page_no, category);
        assert(inserted_in_map);
        static_cast<void>(inserted_in_map);
        try {
            const auto [bucket_iterator, inserted_in_bucket] = buckets_[category].insert(page_no);
            static_cast<void>(bucket_iterator);
            assert(inserted_in_bucket);
            static_cast<void>(inserted_in_bucket);
        } catch (...) {
            page_categories_.erase(category_iterator);
            throw;
        }
        return FsmCandidateIndexError::NONE;
    }
    if (existing->second == category) {
        assert(buckets_[category].contains(page_no));
        return FsmCandidateIndexError::NONE;
    }

    const auto [bucket_iterator, inserted_in_new_bucket] = buckets_[category].insert(page_no);
    static_cast<void>(bucket_iterator);
    assert(inserted_in_new_bucket);
    static_cast<void>(inserted_in_new_bucket);
    const std::size_t erased = buckets_[existing->second].erase(page_no);
    assert(erased == 1U);
    static_cast<void>(erased);
    existing->second = category;
    return FsmCandidateIndexError::NONE;
}

FsmCandidateIndexError FsmCandidateIndex::Remove(PageNo page_no) {
    if (!IsValidHeapPageNumber(page_no)) {
        return FsmCandidateIndexError::INVALID_PAGE_NUMBER;
    }

    const auto existing = page_categories_.find(page_no);
    if (existing == page_categories_.end()) {
        return FsmCandidateIndexError::NONE;
    }

    const std::size_t erased = buckets_[existing->second].erase(page_no);
    assert(erased == 1U);
    static_cast<void>(erased);
    page_categories_.erase(existing);
    return FsmCandidateIndexError::NONE;
}

FsmCandidateResult
FsmCandidateIndex::FindCandidate(std::size_t required_tuple_bytes) const noexcept {
    const auto minimum_category = MinimumFsmCategoryForTupleBytes(required_tuple_bytes);
    if (!minimum_category.category.has_value()) {
        assert(minimum_category.error == FsmTupleRequestError::TUPLE_TOO_LARGE);
        return {
            .page_no = std::nullopt,
            .error = FsmCandidateIndexError::TUPLE_REQUEST_TOO_LARGE,
        };
    }

    const std::size_t first_category = minimum_category.category.value_or(0);
    constexpr std::size_t maximum_category = std::numeric_limits<std::uint8_t>::max();
    for (std::size_t category = first_category; category <= maximum_category; ++category) {
        if (!buckets_[category].empty()) {
            return {
                .page_no = *buckets_[category].begin(),
                .error = FsmCandidateIndexError::NONE,
            };
        }
    }

    return {};
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
