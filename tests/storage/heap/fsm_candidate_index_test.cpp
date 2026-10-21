#include "common/types.h"
#include "storage/heap/fsm_candidate_index.h"
#include "storage/heap/fsm_page.h"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <optional>
#include <random>

namespace dblusblus {
namespace {

[[nodiscard]] std::uint8_t RequireMinimumCategory(std::size_t required_tuple_bytes) {
    const auto result = MinimumFsmCategoryForTupleBytes(required_tuple_bytes);
    if (!result.Ok()) {
        ADD_FAILURE() << "minimum FSM category unexpectedly missing for request "
                      << required_tuple_bytes;
        return 0;
    }
    EXPECT_TRUE(result);
    return *result;
}

[[nodiscard]] std::optional<PageNo> ReferenceCandidate(const std::map<PageNo, std::uint8_t>& pages,
                                                       std::size_t required_tuple_bytes) {
    const auto minimum = MinimumFsmCategoryForTupleBytes(required_tuple_bytes);
    if (!minimum.Ok()) {
        return std::nullopt;
    }

    std::optional<PageNo> candidate;
    std::uint8_t candidate_category = std::numeric_limits<std::uint8_t>::max();
    for (const auto& [page_no, category] : pages) {
        if (category < *minimum) {
            continue;
        }
        if (!candidate.has_value() || category < candidate_category ||
            (category == candidate_category && page_no < *candidate)) {
            candidate = page_no;
            candidate_category = category;
        }
    }
    return candidate;
}

TEST(FsmCandidateIndexTest, TracksAndRemovesOnePage) {
    FsmCandidateIndex index;
    EXPECT_EQ(index.Size(), 0U);
    EXPECT_EQ(index.FindCandidate(0).Value(), std::nullopt);

    EXPECT_TRUE(index.Upsert(7, 100));
    EXPECT_EQ(index.Size(), 1U);
    EXPECT_EQ(index.FindCandidate(FsmCategoryMinimumUsableBytes(100)).Value(),
              std::optional<PageNo>{7});

    EXPECT_TRUE(index.Remove(7));
    EXPECT_EQ(index.Size(), 0U);
    EXPECT_EQ(index.FindCandidate(0).Value(), std::nullopt);
}

TEST(FsmCandidateIndexTest, RejectsInvalidHeapPageNumbersWithoutMutation) {
    FsmCandidateIndex index;
    ASSERT_TRUE(index.Upsert(8, 90));

    EXPECT_EQ(index.Upsert(0, 100).Error(), FsmCandidateIndexError::INVALID_PAGE_NUMBER);
    EXPECT_EQ(index.Upsert(INVALID_PAGE_NO, 100).Error(),
              FsmCandidateIndexError::INVALID_PAGE_NUMBER);
    EXPECT_EQ(index.Remove(0).Error(), FsmCandidateIndexError::INVALID_PAGE_NUMBER);
    EXPECT_EQ(index.Remove(INVALID_PAGE_NO).Error(), FsmCandidateIndexError::INVALID_PAGE_NUMBER);
    EXPECT_EQ(index.Size(), 1U);
    EXPECT_EQ(index.FindCandidate(FsmCategoryMinimumUsableBytes(90)).Value(),
              std::optional<PageNo>{8});
}

TEST(FsmCandidateIndexTest, UpsertMovesOneUniqueMembershipInEitherDirection) {
    FsmCandidateIndex index;

    ASSERT_TRUE(index.Upsert(19, 100));
    ASSERT_TRUE(index.Upsert(19, 100));
    EXPECT_EQ(index.Size(), 1U);

    ASSERT_TRUE(index.Upsert(19, 200));
    EXPECT_EQ(index.Size(), 1U);
    EXPECT_EQ(index.FindCandidate(FsmCategoryMinimumUsableBytes(201)).Value(), std::nullopt);
    EXPECT_EQ(index.FindCandidate(FsmCategoryMinimumUsableBytes(200)).Value(),
              std::optional<PageNo>{19});

    ASSERT_TRUE(index.Upsert(19, 50));
    EXPECT_EQ(index.Size(), 1U);
    EXPECT_EQ(index.FindCandidate(FsmCategoryMinimumUsableBytes(51)).Value(), std::nullopt);
    EXPECT_EQ(index.FindCandidate(FsmCategoryMinimumUsableBytes(50)).Value(),
              std::optional<PageNo>{19});
}

TEST(FsmCandidateIndexTest, SelectsSmallestPageFromSmallestSufficientCategory) {
    FsmCandidateIndex index;
    ASSERT_TRUE(index.Upsert(10, 150));
    ASSERT_TRUE(index.Upsert(2, 150));
    ASSERT_TRUE(index.Upsert(7, 150));
    ASSERT_TRUE(index.Upsert(20, 200));
    ASSERT_TRUE(index.Upsert(5, 100));
    ASSERT_TRUE(index.Upsert(4, 50));

    const std::size_t request = FsmCategoryMinimumUsableBytes(101);
    ASSERT_EQ(RequireMinimumCategory(request), std::uint8_t{101});
    EXPECT_EQ(index.FindCandidate(request).Value(), std::optional<PageNo>{2});

    ASSERT_TRUE(index.Remove(2));
    ASSERT_TRUE(index.Remove(7));
    ASSERT_TRUE(index.Remove(10));
    EXPECT_EQ(index.FindCandidate(request).Value(), std::optional<PageNo>{20});
}

TEST(FsmCandidateIndexTest, ReturnsNoCandidateWhenKnownCategoriesAreInsufficient) {
    FsmCandidateIndex index;
    ASSERT_TRUE(index.Upsert(1, 50));
    ASSERT_TRUE(index.Upsert(2, 100));

    const auto result = index.FindCandidate(FsmCategoryMinimumUsableBytes(101));
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, std::nullopt);
}

TEST(FsmCandidateIndexTest, PinsMinimumCategoryRequestBoundaries) {
    EXPECT_EQ(RequireMinimumCategory(0), std::uint8_t{0});
    EXPECT_EQ(RequireMinimumCategory(1), std::uint8_t{1});
    EXPECT_EQ(RequireMinimumCategory(32), std::uint8_t{1});
    EXPECT_EQ(RequireMinimumCategory(33), std::uint8_t{2});
    EXPECT_EQ(RequireMinimumCategory(4052), std::uint8_t{127});
    EXPECT_EQ(RequireMinimumCategory(4053), std::uint8_t{128});
    EXPECT_EQ(RequireMinimumCategory(8104), std::uint8_t{254});
    EXPECT_EQ(RequireMinimumCategory(8105), std::uint8_t{255});
    EXPECT_EQ(RequireMinimumCategory(FSM_MAX_USABLE_INSERTION_BYTES), std::uint8_t{255});

    const auto oversized = MinimumFsmCategoryForTupleBytes(FSM_MAX_USABLE_INSERTION_BYTES + 1U);
    EXPECT_FALSE(oversized.Ok());
    EXPECT_EQ(oversized.Error(), FsmTupleRequestError::TUPLE_TOO_LARGE);
}

TEST(FsmCandidateIndexTest, MinimumCategoryIsExactAcrossRawTupleDomain) {
    for (std::size_t request = 0; request <= FSM_MAX_USABLE_INSERTION_BYTES; ++request) {
        const auto category = RequireMinimumCategory(request);
        EXPECT_GE(FsmCategoryMinimumUsableBytes(category), request) << "request " << request;
        if (category > 0) {
            EXPECT_LT(FsmCategoryMinimumUsableBytes(static_cast<std::uint8_t>(category - 1U)),
                      request)
                << "request " << request;
        }
    }
}

TEST(FsmCandidateIndexTest, HandlesRequestLimitsAndZeroByteTuples) {
    FsmCandidateIndex index;
    ASSERT_TRUE(index.Upsert(3, 0));
    ASSERT_TRUE(index.Upsert(4, 255));

    const auto zero = index.FindCandidate(0);
    EXPECT_EQ(zero.Value(), std::optional<PageNo>{3});
    EXPECT_TRUE(zero);

    const auto maximum = index.FindCandidate(HEAP_PAGE_MAX_RAW_TUPLE_SIZE);
    EXPECT_EQ(maximum.Value(), std::optional<PageNo>{4});
    EXPECT_TRUE(maximum);

    const auto oversized = index.FindCandidate(HEAP_PAGE_MAX_RAW_TUPLE_SIZE + 1U);
    EXPECT_FALSE(oversized.Ok());
    EXPECT_EQ(oversized.Error(), FsmCandidateIndexError::TUPLE_REQUEST_TOO_LARGE);
}

TEST(FsmCandidateIndexTest, CategoryRepairChangesSubsequentCandidateLookupImmediately) {
    FsmCandidateIndex index;
    ASSERT_TRUE(index.Upsert(10, 200));
    ASSERT_TRUE(index.Upsert(20, 200));
    const std::size_t request = FsmCategoryMinimumUsableBytes(150);
    ASSERT_EQ(index.FindCandidate(request).Value(), std::optional<PageNo>{10});

    ASSERT_TRUE(index.Upsert(10, 120));
    EXPECT_EQ(index.FindCandidate(request).Value(), std::optional<PageNo>{20});

    ASSERT_TRUE(index.Upsert(10, 150));
    EXPECT_EQ(index.FindCandidate(request).Value(), std::optional<PageNo>{10});
}

TEST(FsmCandidateIndexTest, RemoveIsIdempotentAndLeavesOtherPagesAlone) {
    FsmCandidateIndex index;
    ASSERT_TRUE(index.Upsert(5, 80));
    ASSERT_TRUE(index.Upsert(6, 80));

    EXPECT_TRUE(index.Remove(5));
    EXPECT_TRUE(index.Remove(5));
    EXPECT_EQ(index.Size(), 1U);
    EXPECT_EQ(index.FindCandidate(FsmCategoryMinimumUsableBytes(80)).Value(),
              std::optional<PageNo>{6});
}

TEST(FsmCandidateIndexTest, ClearSupportsDeterministicRepeatedUpsertRebuild) {
    FsmCandidateIndex index;
    for (PageNo page_no = 1; page_no <= 100; ++page_no) {
        ASSERT_TRUE(index.Upsert(page_no, static_cast<std::uint8_t>(page_no % 16U)));
    }
    ASSERT_EQ(index.Size(), 100U);

    index.Clear();
    EXPECT_EQ(index.Size(), 0U);
    EXPECT_EQ(index.FindCandidate(0).Value(), std::nullopt);

    ASSERT_TRUE(index.Upsert(9, 70));
    ASSERT_TRUE(index.Upsert(3, 70));
    EXPECT_EQ(index.FindCandidate(FsmCategoryMinimumUsableBytes(70)).Value(),
              std::optional<PageNo>{3});
}

TEST(FsmCandidateIndexTest, RandomizedOperationsMatchReferenceModel) {
    FsmCandidateIndex index;
    std::map<PageNo, std::uint8_t> reference;
    // A fixed seed is intentional: this property test must be reproducible.
    // NOLINTNEXTLINE(bugprone-random-generator-seed)
    std::mt19937_64 random{0xDB1U};
    std::uniform_int_distribution<PageNo> page_distribution{1, 500};
    std::uniform_int_distribution<unsigned int> category_distribution{0, 255};
    std::uniform_int_distribution<std::size_t> request_distribution{
        0, FSM_MAX_USABLE_INSERTION_BYTES + 1U};
    std::uniform_int_distribution<unsigned int> operation_distribution{0, 2};

    for (std::size_t operation_index = 0; operation_index < 20000; ++operation_index) {
        const PageNo page_no = page_distribution(random);
        switch (operation_distribution(random)) {
        case 0: {
            const auto category = static_cast<std::uint8_t>(category_distribution(random));
            ASSERT_TRUE(index.Upsert(page_no, category));
            reference[page_no] = category;
            break;
        }
        case 1:
            ASSERT_TRUE(index.Remove(page_no));
            reference.erase(page_no);
            break;
        case 2: {
            const std::size_t request = request_distribution(random);
            const auto actual = index.FindCandidate(request);
            if (request > FSM_MAX_USABLE_INSERTION_BYTES) {
                EXPECT_EQ(actual.Error(), FsmCandidateIndexError::TUPLE_REQUEST_TOO_LARGE);
                EXPECT_FALSE(actual.Ok());
            } else {
                EXPECT_TRUE(actual);
                EXPECT_EQ(actual.Value(), ReferenceCandidate(reference, request));
            }
            break;
        }
        default:
            FAIL() << "unreachable randomized operation";
        }
        EXPECT_EQ(index.Size(), reference.size());
    }
}

} // namespace
} // namespace dblusblus
