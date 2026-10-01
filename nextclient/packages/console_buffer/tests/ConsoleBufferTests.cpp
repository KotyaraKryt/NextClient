#include <gtest/gtest.h>

#include <console_buffer/console_buffer.h>

using console_buffer::ConsoleBuffer;
using console_buffer::Line;
using console_buffer::Rgba;

static const Rgba kWhite = { 255, 255, 255, 255 };
static const Rgba kRed = { 255, 0, 0, 255 };

static std::string Text(const Line& line)
{
    std::string text;
    for (const auto& segment : line.segments)
        text += segment.text;

    return text;
}

TEST(ConsoleBuffer, SplitsTextIntoLines)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "first\nsecond\n");

    ASSERT_EQ(buffer.Lines().size(), 2u);
    EXPECT_EQ(Text(buffer.Lines()[0]), "first");
    EXPECT_EQ(Text(buffer.Lines()[1]), "second");
}

TEST(ConsoleBuffer, JoinsPiecesOfOneLine)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "] echo ");
    buffer.Print(kWhite, "hi");
    buffer.Print(kWhite, "\n");

    ASSERT_EQ(buffer.Lines().size(), 1u);
    EXPECT_EQ(Text(buffer.Lines()[0]), "] echo hi");
}

TEST(ConsoleBuffer, ShowsALineBeforeItsLineBreakArrives)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "Connecting...");

    ASSERT_EQ(buffer.Lines().size(), 1u);
    EXPECT_EQ(Text(buffer.Lines()[0]), "Connecting...");
}

TEST(ConsoleBuffer, StartsANewLineOnlyAfterALineBreak)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "one\n");
    buffer.Print(kWhite, "two");

    ASSERT_EQ(buffer.Lines().size(), 2u);
    EXPECT_EQ(Text(buffer.Lines()[1]), "two");
}

TEST(ConsoleBuffer, KeepsEmptyLines)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "a\n\nb\n");

    ASSERT_EQ(buffer.Lines().size(), 3u);
    EXPECT_EQ(Text(buffer.Lines()[0]), "a");
    EXPECT_EQ(Text(buffer.Lines()[1]), "");
    EXPECT_EQ(Text(buffer.Lines()[2]), "b");
}

TEST(ConsoleBuffer, KeepsColorChangesWithinALine)
{
    ConsoleBuffer buffer;
    buffer.Print(kRed, "Blocked stufftext cmd (by engine): ");
    buffer.Print(kWhite, "fullserverinfo\n");

    ASSERT_EQ(buffer.Lines().size(), 1u);
    const auto& segments = buffer.Lines()[0].segments;
    ASSERT_EQ(segments.size(), 2u);
    EXPECT_EQ(segments[0].color, kRed);
    EXPECT_EQ(segments[1].color, kWhite);
    EXPECT_EQ(segments[1].text, "fullserverinfo");
}

TEST(ConsoleBuffer, MergesPiecesOfTheSameColor)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "ab");
    buffer.Print(kWhite, "cd\n");

    ASSERT_EQ(buffer.Lines().size(), 1u);
    ASSERT_EQ(buffer.Lines()[0].segments.size(), 1u);
    EXPECT_EQ(buffer.Lines()[0].segments[0].text, "abcd");
}

TEST(ConsoleBuffer, DropsTheOldestLinesPastTheLimit)
{
    ConsoleBuffer buffer(2);
    buffer.Print(kWhite, "1\n2\n3\n");

    ASSERT_EQ(buffer.Lines().size(), 2u);
    EXPECT_EQ(Text(buffer.Lines()[0]), "2");
    EXPECT_EQ(Text(buffer.Lines()[1]), "3");
}

TEST(ConsoleBuffer, ClearForgetsAnUnfinishedLine)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "half");
    buffer.Clear();
    buffer.Print(kWhite, "new\n");

    ASSERT_EQ(buffer.Lines().size(), 1u);
    EXPECT_EQ(Text(buffer.Lines()[0]), "new");
}
