#include <gtest/gtest.h>

#include <console_buffer/kinds.h>

using namespace console_buffer;

static const Rgba kWhite = { 255, 255, 255, 255 };

TEST(Classify, RecognizesErrorsAndWarnings)
{
    EXPECT_EQ(Classify(Source::Normal, "Error: server failed to transmit file 'sound/x.wav'"), Kind::Error);
    EXPECT_EQ(Classify(Source::Normal, "Connection failed after 4 retries"), Kind::Error);
    EXPECT_EQ(Classify(Source::Normal, "Warning: model not precached"), Kind::Warning);
    EXPECT_EQ(Classify(Source::Normal, "Couldn't open file overviews/jb_mini.txt. Using default values for overiew mode."), Kind::Warning);
    EXPECT_EQ(Classify(Source::Normal, "[HTTP] Can't download: sound/vehicles/diesel_start1.wav | HTTP Code: 404"), Kind::Warning);
    EXPECT_EQ(Classify(Source::Normal, "Unknown command: fullserverinfo"), Kind::Warning);
}

TEST(Classify, RecognizesBlockedServerCommands)
{
    EXPECT_EQ(Classify(Source::Normal, "Blocked stufftext cmd (by engine): fullserverinfo"), Kind::Blocked);
    EXPECT_EQ(Classify(Source::Normal, "(line breaks and tabulation are displayed as \\n and \\t, respectively)"), Kind::Blocked);
}

TEST(Classify, RecognizesTypedCommands)
{
    EXPECT_EQ(Classify(Source::Normal, "] fps_max 100"), Kind::Command);
}

TEST(Classify, TrustsTheSourceOverTheText)
{
    EXPECT_EQ(Classify(Source::Chat, "kotya : Error is my middle name"), Kind::Chat);
    EXPECT_EQ(Classify(Source::Developer, "Error: missing sprite"), Kind::Developer);
}

TEST(Classify, CallsTheRestInfo)
{
    EXPECT_EQ(Classify(Source::Normal, "Connecting to 46.174.53.199:27015..."), Kind::Info);
    EXPECT_EQ(Classify(Source::Normal, ""), Kind::Info);
}

TEST(ConsoleBufferKinds, ClassifiesTheWholeLineAsItArrives)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "Err");
    EXPECT_EQ(buffer.Lines()[0].kind, Kind::Info);

    buffer.Print(kWhite, "or: something\n");
    EXPECT_EQ(buffer.Lines()[0].kind, Kind::Error);
}

TEST(ConsoleBufferKinds, MarksTheNextLineAsChat)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "before\n");
    buffer.MarkNextLine(Source::Chat);
    buffer.Print(kWhite, "kotya");
    buffer.Print(kWhite, ": hi\n");
    buffer.Print(kWhite, "after\n");

    EXPECT_EQ(buffer.Lines()[0].kind, Kind::Info);
    EXPECT_EQ(buffer.Lines()[1].kind, Kind::Chat);
    EXPECT_EQ(buffer.Lines()[2].kind, Kind::Info);
}

TEST(ConsoleBufferKinds, KeepsDeveloperTextApart)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "texture load\n", false, Source::Developer);

    EXPECT_EQ(buffer.Lines()[0].kind, Kind::Developer);
}

TEST(ConsoleBufferKinds, KeepsThemedAndColoredPiecesApart)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "a", true);
    buffer.Print(kWhite, "b\n", false);

    ASSERT_EQ(buffer.Lines()[0].segments.size(), 2u);
    EXPECT_TRUE(buffer.Lines()[0].segments[0].themed);
    EXPECT_FALSE(buffer.Lines()[0].segments[1].themed);
}

TEST(ConsoleBufferKinds, CountsChanges)
{
    ConsoleBuffer buffer;
    uint64_t before = buffer.Generation();
    buffer.Print(kWhite, "x");
    EXPECT_NE(buffer.Generation(), before);
}

TEST(Search, IgnoresCaseInLatinAndCyrillic)
{
    EXPECT_EQ(ToLowerUtf8("De_Dust2"), "de_dust2");
    EXPECT_EQ(ToLowerUtf8("ПРИВЕТ Ёжик"), "привет ёжик");
    EXPECT_EQ(ToLowerUtf8("АБВГДЕЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ"), "абвгдежзийклмнопрстуфхцчшщъыьэюя");
    EXPECT_TRUE(ContainsLowered("Привет, Мир", "мир"));
    EXPECT_FALSE(ContainsLowered("Привет", "пока"));
}
