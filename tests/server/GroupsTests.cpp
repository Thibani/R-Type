// The rules of groups, as plain data: no network, no time.

#include "server/Groups.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>

using server::Group;
using server::Groups;
using Outcome = server::Groups::Outcome;
using StartProblem = server::Groups::StartProblem;

namespace {

/// `Lea` leads a group with the code ABC234, `Max` is in it, `Zoe` is alone.
struct Three {
    Three()
    {
        groups.add("Lea");
        groups.add("Max");
        groups.add("Zoe");
        groups.of("Lea")->code = "ABC234";
        EXPECT_EQ(groups.join("Max", "ABC234"), std::nullopt);
    }

    Groups groups;
};

} // namespace

TEST(Groups, ANewPlayerLeadsTheirOwnGroup)
{
    Groups groups;
    groups.add("Lea");

    const Group* group = groups.of("Lea");

    ASSERT_NE(group, nullptr);
    EXPECT_EQ(group->leader, "Lea");
    EXPECT_EQ(group->members.size(), 1U);
    EXPECT_TRUE(group->code.empty());
}

TEST(Groups, JoiningMovesAPlayerAndLeavesNoEmptyGroup)
{
    const Three three;
    EXPECT_EQ(three.groups.all().size(), 2U);
}

TEST(Groups, AnEmptyCodeOpensNothing)
{
    Three three;
    EXPECT_EQ(three.groups.join("Zoe", ""), Outcome::UnknownCode); // Zoe's own code is empty too
    EXPECT_EQ(three.groups.join("Zoe", "ZZZZZZ"), Outcome::UnknownCode);
}

TEST(Groups, AFullGroupRefuses)
{
    Three three;
    three.groups.add("Tom");
    three.groups.add("Ann");
    EXPECT_EQ(three.groups.join("Zoe", "ABC234"), std::nullopt);
    EXPECT_EQ(three.groups.join("Tom", "ABC234"), std::nullopt);

    EXPECT_EQ(three.groups.join("Ann", "ABC234"), Outcome::GroupFull);
    EXPECT_EQ(three.groups.of("Lea")->members.size(), 4U);
}

TEST(Groups, JoiningYourOwnGroupChangesNothing)
{
    Three three;
    EXPECT_EQ(three.groups.join("Max", "ABC234"), std::nullopt);
    EXPECT_EQ(three.groups.of("Lea")->members.size(), 2U);
}

TEST(Groups, AGroupInAGameRefusesNewcomers)
{
    Three three;
    three.groups.of("Lea")->members.at(1).inGame = true;
    EXPECT_EQ(three.groups.join("Zoe", "ABC234"), Outcome::GroupInGame);
}

TEST(Groups, LeavingAloneIsRefused)
{
    Three three;
    EXPECT_EQ(three.groups.leave("Zoe"), Outcome::Alone);
}

TEST(Groups, AMemberWhoLeavesIsAloneAndLeading)
{
    Three three;
    EXPECT_EQ(three.groups.leave("Max"), std::nullopt);
    EXPECT_EQ(three.groups.of("Max")->leader, "Max");
    EXPECT_EQ(three.groups.of("Lea")->members.size(), 1U);
}

TEST(Groups, TheRolePassesOnWhenTheLeaderLeaves)
{
    Three three;
    EXPECT_EQ(three.groups.leave("Lea"), std::nullopt);
    EXPECT_EQ(three.groups.of("Max")->leader, "Max");
    EXPECT_EQ(three.groups.of("Max")->code, "ABC234"); // The group stays the same
}

TEST(Groups, TheRolePassesOnWhenTheLeaderGoesOffline)
{
    Three three;
    three.groups.remove("Lea");
    EXPECT_EQ(three.groups.of("Lea"), nullptr);
    EXPECT_EQ(three.groups.of("Max")->leader, "Max");
}

TEST(Groups, TheLastOneOfflineTakesTheGroupAndItsCodeAway)
{
    Three three;
    three.groups.remove("Lea");
    three.groups.remove("Max");
    EXPECT_FALSE(three.groups.codeInUse("ABC234"));
    EXPECT_EQ(three.groups.all().size(), 1U);
}

TEST(Groups, KickNeedsTheLeaderAndAMember)
{
    Three three;
    EXPECT_EQ(three.groups.kick("Max", "Lea"), Outcome::NotLeader);
    EXPECT_EQ(three.groups.kick("Lea", "Zoe"), Outcome::NotMember);
    EXPECT_EQ(three.groups.kick("Lea", "Lea"), Outcome::NotMember);
    EXPECT_EQ(three.groups.of("Lea")->members.size(), 2U);
}

TEST(Groups, APlayerInAGameCannotBeKicked)
{
    Three three;
    three.groups.of("Lea")->members.at(1).inGame = true;
    EXPECT_EQ(three.groups.kick("Lea", "Max"), Outcome::GroupInGame);
}

TEST(Groups, AKickedPlayerIsAloneAndTheCodeRefusesThem)
{
    Three three;
    EXPECT_EQ(three.groups.kick("Lea", "Max"), std::nullopt);
    EXPECT_EQ(three.groups.of("Max")->members.size(), 1U);
    EXPECT_EQ(three.groups.of("Max")->leader, "Max");
    EXPECT_EQ(three.groups.join("Max", "ABC234"), Outcome::Kicked);
}

TEST(Groups, AnInvitationLiftsAKick)
{
    Three three;
    (void)three.groups.kick("Lea", "Max");

    EXPECT_EQ(three.groups.invite("Lea", "Max"), std::nullopt);
    EXPECT_EQ(three.groups.accept("Max", "Lea"), std::nullopt);

    EXPECT_TRUE(three.groups.of("Lea")->hasMember("Max"));
}

TEST(Groups, PromoteNeedsTheLeaderAndAMember)
{
    Three three;
    EXPECT_EQ(three.groups.promote("Max", "Max"), Outcome::NotLeader);
    EXPECT_EQ(three.groups.promote("Lea", "Zoe"), Outcome::NotMember);
    EXPECT_EQ(three.groups.promote("Lea", "Max"), std::nullopt);
    EXPECT_EQ(three.groups.of("Lea")->leader, "Max");
}

TEST(Groups, OnlyTheLeaderInvites)
{
    Three three;
    EXPECT_EQ(three.groups.invite("Max", "Zoe"), Outcome::NotLeader);
    EXPECT_EQ(three.groups.invite("Lea", "Max"), Outcome::NotAvailable); // Already in
}

TEST(Groups, AnAcceptWithoutAnInvitationIsRefused)
{
    Three three;
    EXPECT_EQ(three.groups.accept("Zoe", "Lea"), Outcome::InvitationGone);
    EXPECT_EQ(three.groups.accept("Zoe", "Nobody"), Outcome::InvitationGone);
}

TEST(Groups, AnInvitationServesOnce)
{
    Three three;
    (void)three.groups.invite("Lea", "Zoe");
    EXPECT_EQ(three.groups.accept("Zoe", "Lea"), std::nullopt);
    EXPECT_EQ(three.groups.leave("Zoe"), std::nullopt);

    EXPECT_EQ(three.groups.accept("Zoe", "Lea"), Outcome::InvitationGone);
}

TEST(Groups, ARefusedInvitationIsGone)
{
    Three three;
    (void)three.groups.invite("Lea", "Zoe");
    three.groups.refuse("Zoe", "Lea");
    EXPECT_EQ(three.groups.accept("Zoe", "Lea"), Outcome::InvitationGone);
}

TEST(Groups, AnInvitationGoesWithWhoLeftTheGroup)
{
    Three three;
    (void)three.groups.invite("Lea", "Zoe");
    EXPECT_EQ(three.groups.leave("Lea"), std::nullopt);
    EXPECT_EQ(three.groups.accept("Zoe", "Lea"), Outcome::InvitationGone); // Lea's new group
}

TEST(Groups, AnInvitationToAFullGroupIsRefused)
{
    Three three;
    three.groups.add("Tom");
    three.groups.add("Ann");
    (void)three.groups.invite("Lea", "Ann");
    EXPECT_EQ(three.groups.join("Zoe", "ABC234"), std::nullopt);
    EXPECT_EQ(three.groups.join("Tom", "ABC234"), std::nullopt);
    EXPECT_EQ(three.groups.accept("Ann", "Lea"), Outcome::GroupFull);
    EXPECT_EQ(three.groups.invite("Lea", "Ann"), Outcome::GroupFull);
}

TEST(Groups, StartNeedsTheLeaderAndEveryoneElseReady)
{
    Three three;
    EXPECT_EQ(three.groups.startProblem("Max"), StartProblem::NotLeader);
    EXPECT_EQ(three.groups.startProblem("Lea"), StartProblem::NotReady);

    three.groups.setReady("Max", true);
    EXPECT_EQ(three.groups.startProblem("Lea"), StartProblem::None);
    EXPECT_EQ(three.groups.startProblem("Zoe"), StartProblem::None); // Alone
}

TEST(Groups, AMemberStillInAGameStopsTheStart)
{
    Three three;
    three.groups.setReady("Max", true);
    three.groups.of("Lea")->members.at(1).inGame = true;
    EXPECT_EQ(three.groups.startProblem("Lea"), StartProblem::NotReady);
}

/// Every change of who is in the group, or of who is ready, must stop the countdown.
class CountdownStops : public testing::Test {
protected:
    void SetUp() override
    {
        three.groups.setReady("Max", true);
        three.groups.of("Lea")->countdown = 3.0F;
    }

    Three three;

    [[nodiscard]]
    bool running()
    {
        return three.groups.of("Lea")->countdown.has_value();
    }
};

TEST_F(CountdownStops, WhenSomeoneTakesBackReady)
{
    three.groups.setReady("Max", false);
    EXPECT_FALSE(running());
}

TEST_F(CountdownStops, ButNotWhenAReadyPlayerSaysItAgain)
{
    three.groups.setReady("Max", true);
    EXPECT_TRUE(running());
}

TEST_F(CountdownStops, WhenSomeoneArrives)
{
    EXPECT_EQ(three.groups.join("Zoe", "ABC234"), std::nullopt);
    EXPECT_FALSE(running());
}

TEST_F(CountdownStops, WhenSomeoneLeaves)
{
    EXPECT_EQ(three.groups.leave("Max"), std::nullopt);
    EXPECT_FALSE(running());
}

TEST_F(CountdownStops, WhenSomeoneGoesOffline)
{
    three.groups.remove("Max");
    EXPECT_FALSE(running());
}

TEST_F(CountdownStops, WhenSomeoneIsKicked)
{
    EXPECT_EQ(three.groups.kick("Lea", "Max"), std::nullopt);
    EXPECT_FALSE(running());
}

TEST_F(CountdownStops, WhenTheRoleChanges)
{
    EXPECT_EQ(three.groups.promote("Lea", "Max"), std::nullopt);
    EXPECT_FALSE(running());
}

TEST(Groups, ANewMemberIsNotReady)
{
    Three three;
    (void)three.groups.join("Zoe", "ABC234");
    EXPECT_EQ(three.groups.startProblem("Lea"), StartProblem::NotReady);
}
