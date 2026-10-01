#include <gtest/gtest.h>

#include <console_buffer/command_history.h>

using console_buffer::CommandHistory;

TEST(CommandHistory, RemembersCommandsOldestFirst)
{
    CommandHistory history;
    history.Add("connect 1.2.3.4");
    history.Add("fps_max 100");

    ASSERT_EQ(history.Entries().size(), 2u);
    EXPECT_EQ(history.Entries()[0], "connect 1.2.3.4");
    EXPECT_EQ(history.Entries()[1], "fps_max 100");
}

TEST(CommandHistory, SkipsEmptyCommandsAndRepeats)
{
    CommandHistory history;
    history.Add("status");
    history.Add("status");
    history.Add("");
    history.Add("retry");
    history.Add("status");

    ASSERT_EQ(history.Entries().size(), 3u);
    EXPECT_EQ(history.Entries()[2], "status");
}

TEST(CommandHistory, ForgetsTheOldestPastTheLimit)
{
    CommandHistory history(2);
    history.Add("a");
    history.Add("b");
    history.Add("c");

    ASSERT_EQ(history.Entries().size(), 2u);
    EXPECT_EQ(history.Entries()[0], "b");
    EXPECT_EQ(history.Entries()[1], "c");
}

TEST(CommandHistory, UpWalksBackAndStopsAtTheOldest)
{
    CommandHistory history;
    history.Add("a");
    history.Add("b");

    EXPECT_EQ(history.Older(), "b");
    EXPECT_EQ(history.Older(), "a");
    EXPECT_EQ(history.Older(), "a");
}

TEST(CommandHistory, DownComesBackToAnEmptyLine)
{
    CommandHistory history;
    history.Add("a");
    history.Add("b");

    history.Older();
    history.Older();
    EXPECT_EQ(history.Newer(), "b");
    EXPECT_EQ(history.Newer(), "");
    EXPECT_EQ(history.Newer(), std::nullopt);
}

TEST(CommandHistory, DownDoesNothingBeforeUp)
{
    CommandHistory history;
    history.Add("a");

    EXPECT_EQ(history.Newer(), std::nullopt);
}

TEST(CommandHistory, UpDoesNothingWithoutHistory)
{
    CommandHistory history;

    EXPECT_EQ(history.Older(), std::nullopt);
}

TEST(CommandHistory, AddingStopsTheWalk)
{
    CommandHistory history;
    history.Add("a");
    history.Add("b");

    history.Older();
    history.Older();
    history.Add("c");

    EXPECT_EQ(history.Older(), "c");
}

TEST(CommandHistory, SurvivesSaveAndLoad)
{
    CommandHistory saved;
    saved.Add("connect 1.2.3.4");
    saved.Add("name kotya");

    CommandHistory loaded;
    loaded.Load(saved.Save());

    EXPECT_EQ(loaded.Entries(), saved.Entries());
    EXPECT_EQ(loaded.Older(), "name kotya");
}

TEST(CommandHistory, LoadsWindowsLineEnds)
{
    CommandHistory history;
    history.Load("status\r\nretry\r\n");

    ASSERT_EQ(history.Entries().size(), 2u);
    EXPECT_EQ(history.Entries()[0], "status");
    EXPECT_EQ(history.Entries()[1], "retry");
}
