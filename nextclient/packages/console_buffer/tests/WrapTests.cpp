#include <gtest/gtest.h>

#include <console_buffer/selection.h>

using namespace console_buffer;
using Rows = std::vector<Row>;

TEST(WrapText, LeavesShortTextWhole)
{
    EXPECT_EQ(WrapText("fps_max 100", 20), (Rows{ { 0, 0, 11 } }));
    EXPECT_EQ(WrapText("", 20), (Rows{ { 0, 0, 0 } }));
}

TEST(WrapText, SplitsAfterTheLastSpaceThatFits)
{
    // "kotya killed " is cut before "rauf", and "rauf with ak47" just fits the next row
    EXPECT_EQ(WrapText("kotya killed rauf with ak47", 14), (Rows{ { 0, 0, 13 }, { 0, 13, 27 } }));
    EXPECT_EQ(WrapText("kotya killed rauf with ak47", 10), (Rows{ { 0, 0, 6 }, { 0, 6, 13 }, { 0, 13, 23 }, { 0, 23, 27 } }));
}

TEST(WrapText, CutsAWordLongerThanARow)
{
    EXPECT_EQ(WrapText("https://t.me/dog_spb", 8), (Rows{ { 0, 0, 8 }, { 0, 8, 16 }, { 0, 16, 20 } }));
}

TEST(WrapText, CountsCharactersNotBytes)
{
    // 12 Cyrillic characters, 24 bytes
    EXPECT_EQ(WrapText("Привет Привет", 7), (Rows{ { 0, 0, 7 }, { 0, 7, 13 } }));
}

TEST(WrapText, KeepsLinesWholeWithoutAWidth)
{
    EXPECT_EQ(WrapText("a b c d e f g", 0), (Rows{ { 0, 0, 13 } }));
}

TEST(WrapLines, NumbersTheRowsByTheirLine)
{
    Line first, second;
    first.text = "aaaa bbbb";
    second.text = "cc";
    LineView lines = { &first, &second };

    EXPECT_EQ(WrapLines(lines, 5), (Rows{ { 0, 0, 5 }, { 0, 5, 9 }, { 1, 0, 2 } }));
}

TEST(PositionAtRow, GivesTheColumnInTheWholeLine)
{
    Line line;
    line.text = "aaaa bbbb";
    LineView lines = { &line };
    Rows rows = WrapLines(lines, 5);

    // the second row starts at column 5 of the line; each row is 20 px tall, characters 10 px wide
    EXPECT_EQ(PositionAtRow(lines, rows, 21, 25, 10, 20), (TextPos{ 0, 7 }));
    EXPECT_EQ(PositionAtRow(lines, rows, 500, 25, 10, 20), (TextPos{ 0, 9 }));
    EXPECT_EQ(PositionAtRow(lines, rows, -5, 5, 10, 20), (TextPos{ 0, 0 }));
}
