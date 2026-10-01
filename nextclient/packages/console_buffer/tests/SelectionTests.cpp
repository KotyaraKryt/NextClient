#include <gtest/gtest.h>

#include <console_buffer/selection.h>

using namespace console_buffer;

static const Rgba kWhite = { 255, 255, 255, 255 };
static const Rgba kRed = { 255, 0, 0, 255 };

// lines 10 px wide per character and 20 px tall, as if the font were that big
static const float kCharWidth = 10.0f;
static const float kLineHeight = 20.0f;

static ConsoleBuffer ThreeLines()
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "de_dust2\n");
    buffer.Print(kWhite, "Привет\n");
    buffer.Print(kWhite, "ok\n");
    return buffer;
}

TEST(Utf8, CountsCharactersNotBytes)
{
    EXPECT_EQ(CharCount("de_dust2"), 8);
    EXPECT_EQ(CharCount("Привет"), 6);
    EXPECT_EQ(CharCount(""), 0);
}

TEST(Utf8, FindsTheByteWhereACharacterStarts)
{
    EXPECT_EQ(ByteOffset("Привет", 0), 0u);
    EXPECT_EQ(ByteOffset("Привет", 2), 4u);
    EXPECT_EQ(ByteOffset("Привет", 6), 12u);
    EXPECT_EQ(ByteOffset("Привет", 99), 12u);
}

TEST(LineText, JoinsTheSegments)
{
    ConsoleBuffer buffer;
    buffer.Print(kRed, "Blocked: ");
    buffer.Print(kWhite, "cmd\n");

    EXPECT_EQ(LineText(buffer.Lines()[0]), "Blocked: cmd");
}

TEST(SelectedText, TakesPartOfOneLine)
{
    ConsoleBuffer buffer = ThreeLines();
    EXPECT_EQ(SelectedText(buffer, { 0, 3 }, { 0, 8 }), "dust2");
}

TEST(SelectedText, CutsCyrillicByCharacters)
{
    ConsoleBuffer buffer = ThreeLines();
    EXPECT_EQ(SelectedText(buffer, { 1, 1 }, { 1, 4 }), "рив");
}

TEST(SelectedText, JoinsSeveralLines)
{
    ConsoleBuffer buffer = ThreeLines();
    EXPECT_EQ(SelectedText(buffer, { 0, 3 }, { 2, 1 }), "dust2\nПривет\no");
}

TEST(SelectedText, DoesNotCareWhichEndCameFirst)
{
    ConsoleBuffer buffer = ThreeLines();
    EXPECT_EQ(SelectedText(buffer, { 2, 1 }, { 0, 3 }), "dust2\nПривет\no");
}

TEST(SelectedText, IsEmptyWhenBothEndsMeet)
{
    ConsoleBuffer buffer = ThreeLines();
    EXPECT_EQ(SelectedText(buffer, { 1, 2 }, { 1, 2 }), "");
}

TEST(PositionAt, FindsTheLineUnderThePoint)
{
    ConsoleBuffer buffer = ThreeLines();
    EXPECT_EQ(PositionAt(buffer, 0, 5, kCharWidth, kLineHeight).line, 0);
    EXPECT_EQ(PositionAt(buffer, 0, 25, kCharWidth, kLineHeight).line, 1);
    EXPECT_EQ(PositionAt(buffer, 0, 59, kCharWidth, kLineHeight).line, 2);
}

TEST(PositionAt, PicksTheNearestGapBetweenCharacters)
{
    ConsoleBuffer buffer = ThreeLines();
    // the left part of the third character is closer to the gap before it, the right part to the one after
    EXPECT_EQ(PositionAt(buffer, 24, 5, kCharWidth, kLineHeight).column, 2);
    EXPECT_EQ(PositionAt(buffer, 26, 5, kCharWidth, kLineHeight).column, 3);
}

TEST(PositionAt, StopsAtTheEndsOfALine)
{
    ConsoleBuffer buffer = ThreeLines();
    EXPECT_EQ(PositionAt(buffer, -30, 5, kCharWidth, kLineHeight).column, 0);
    EXPECT_EQ(PositionAt(buffer, 500, 5, kCharWidth, kLineHeight).column, 8);
    // "Привет" is 12 bytes but 6 characters
    EXPECT_EQ(PositionAt(buffer, 500, 25, kCharWidth, kLineHeight).column, 6);
}

TEST(PositionAt, StopsAtTheFirstAndLastLines)
{
    ConsoleBuffer buffer = ThreeLines();
    EXPECT_EQ(PositionAt(buffer, 0, -40, kCharWidth, kLineHeight).line, 0);
    EXPECT_EQ(PositionAt(buffer, 0, 900, kCharWidth, kLineHeight).line, 2);
}

TEST(PositionAt, HandlesAnEmptyBuffer)
{
    ConsoleBuffer buffer;
    TextPos pos = PositionAt(buffer, 50, 50, kCharWidth, kLineHeight);
    EXPECT_EQ(pos.line, 0);
    EXPECT_EQ(pos.column, 0);
}
