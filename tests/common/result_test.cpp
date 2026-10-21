#include "common/result.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace dblusblus {
namespace {

enum class TestError : std::uint8_t {
    FIRST,
    SECOND,
};

Result<int, TestError> Half(int value) {
    if (value % 2 != 0) {
        return Unexpected{TestError::FIRST};
    }
    return value / 2;
}

Status<TestError> CheckPositive(int value) {
    if (value <= 0) {
        return Unexpected{TestError::SECOND};
    }
    return {};
}

TEST(ResultTest, HoldsValueOnSuccess) {
    const auto result = Half(8);
    ASSERT_TRUE(result.Ok());
    EXPECT_TRUE(static_cast<bool>(result));
    EXPECT_EQ(result.Value(), 4);
    EXPECT_EQ(*result, 4);
    EXPECT_EQ(result.AsOptional(), std::optional<int>{4});
}

TEST(ResultTest, HoldsErrorOnFailure) {
    const auto result = Half(7);
    ASSERT_FALSE(result.Ok());
    EXPECT_FALSE(static_cast<bool>(result));
    EXPECT_EQ(result.Error(), TestError::FIRST);
    EXPECT_EQ(result.AsOptional(), std::nullopt);
}

TEST(ResultTest, SupportsMoveOnlyValuesAndStructuredErrors) {
    struct Detail {
        TestError code{TestError::FIRST};
        std::string message;
    };
    Result<std::unique_ptr<int>, Detail> success{std::make_unique<int>(3)};
    ASSERT_TRUE(success);
    const std::unique_ptr<int> owned = std::move(success).Value();
    EXPECT_EQ(*owned, 3);

    const Result<std::unique_ptr<int>, Detail> failure =
        Unexpected{Detail{.code = TestError::SECOND, .message = "bad"}};
    ASSERT_FALSE(failure);
    EXPECT_EQ(failure.Error().code, TestError::SECOND);
    EXPECT_EQ(failure.Error().message, "bad");
}

TEST(ResultTest, ArrowAccessesTheValue) {
    const Result<std::string, TestError> result{std::string{"abc"}};
    EXPECT_EQ(result->size(), 3U);
}

TEST(StatusTest, DefaultIsSuccessAndUnexpectedIsFailure) {
    EXPECT_TRUE(CheckPositive(1).Ok());
    const auto failure = CheckPositive(0);
    ASSERT_FALSE(failure);
    EXPECT_EQ(failure.Error(), TestError::SECOND);
}

#ifndef NDEBUG
TEST(ResultDeathTest, WrongAlternativeAccessAborts) {
    const auto failure = Half(1);
    EXPECT_DEATH(static_cast<void>(failure.Value()),
                 "Result::Value\\(\\) called on an error result");
    const auto success = Half(2);
    EXPECT_DEATH(static_cast<void>(success.Error()), "called on a successful result");
}

TEST(AssertDeathTest, AssertReportsConditionAndMessage) {
    const int value = 1;
    EXPECT_DEATH(DBLUS_ASSERT(value == 2, "value must be two"), "value must be two");
}
#endif

TEST(AssertDeathTest, CheckIsAlwaysOn) {
    const int value = 1;
    EXPECT_DEATH(DBLUS_CHECK(value == 2, "always checked"), "always checked");
    DBLUS_CHECK(value == 1, "passes silently");
}

} // namespace
} // namespace dblusblus
