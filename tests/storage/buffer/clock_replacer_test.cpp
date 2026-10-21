#include "storage/buffer/clock_replacer.h"

#include <gtest/gtest.h>
#include <optional>
#include <vector>

namespace dblusblus::storage {
namespace {

TEST(ClockReplacerTest, ChoosesFirstEligibleUnreferencedFrameFromTheHand) {
    ClockReplacer replacer{4};
    const auto victim = replacer.FindCandidate([](FrameId) { return true; });
    EXPECT_EQ(victim, std::optional<FrameId>{0U});
    EXPECT_EQ(replacer.Hand(), 1U);
    EXPECT_EQ(replacer.LastPassVisits(), 1U);
}

TEST(ClockReplacerTest, GivesReferencedFramesASecondChance) {
    ClockReplacer replacer{3};
    replacer.SetReferenced(0);
    replacer.SetReferenced(1);
    const auto victim = replacer.FindCandidate([](FrameId) { return true; });
    EXPECT_EQ(victim, std::optional<FrameId>{2U});
    EXPECT_FALSE(replacer.IsReferenced(0));
    EXPECT_FALSE(replacer.IsReferenced(1));
}

TEST(ClockReplacerTest, ReturnsAClearedFrameOnTheSecondRotationWhenAllAreReferenced) {
    ClockReplacer replacer{3};
    for (FrameId frame = 0; frame < 3U; ++frame) {
        replacer.SetReferenced(frame);
    }
    const auto victim = replacer.FindCandidate([](FrameId) { return true; });
    EXPECT_EQ(victim, std::optional<FrameId>{0U});
    EXPECT_EQ(replacer.LastPassVisits(), 4U);
}

TEST(ClockReplacerTest, SkipsIneligibleFramesWithoutClearingTheirReferenceBit) {
    ClockReplacer replacer{3};
    replacer.SetReferenced(0);
    const auto victim = replacer.FindCandidate([](FrameId frame) { return frame == 2U; });
    EXPECT_EQ(victim, std::optional<FrameId>{2U});
    EXPECT_TRUE(replacer.IsReferenced(0));
}

TEST(ClockReplacerTest, OneUnsuccessfulAttemptVisitsEachFrameAtMostTwice) {
    ClockReplacer replacer{5};
    std::vector<FrameId> visited;
    const auto victim = replacer.FindCandidate([&](FrameId frame) {
        visited.push_back(frame);
        return false;
    });
    EXPECT_FALSE(victim.has_value());
    EXPECT_EQ(replacer.LastPassVisits(), 10U);
    EXPECT_EQ(visited.size(), 10U);
}

TEST(ClockReplacerTest, ContinuesFromTheHandAcrossAttempts) {
    ClockReplacer replacer{3};
    EXPECT_EQ(replacer.FindCandidate([](FrameId) { return true; }), FrameId{0});
    EXPECT_EQ(replacer.FindCandidate([](FrameId) { return true; }), FrameId{1});
    EXPECT_EQ(replacer.FindCandidate([](FrameId) { return true; }), FrameId{2});
    EXPECT_EQ(replacer.FindCandidate([](FrameId) { return true; }), FrameId{0});
}

} // namespace
} // namespace dblusblus::storage
