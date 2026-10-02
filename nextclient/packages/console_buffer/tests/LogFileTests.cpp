#include <gtest/gtest.h>

#include <console_buffer/log_file.h>

#include <ctime>
#include <vector>

using namespace console_buffer;

static const Rgba kWhite = { 255, 255, 255, 255 };

// 2026-10-02 10:59:06 in the local time zone, whatever it is where the tests run
static int64_t SampleTime()
{
    std::tm local = {};
    local.tm_year = 2026 - 1900;
    local.tm_mon = 9;
    local.tm_mday = 2;
    local.tm_hour = 10;
    local.tm_min = 59;
    local.tm_sec = 6;
    local.tm_isdst = -1;
    return static_cast<int64_t>(std::mktime(&local));
}

static Line LoggedLine(Topic topic, const std::string& text)
{
    Line line;
    line.text = text;
    line.segments.push_back({ kWhite, text, true });
    line.time = SampleTime();
    line.topic = topic;
    return line;
}

TEST(LogFile, WritesTheDateTheGroupAndTheText)
{
    EXPECT_EQ(FormatLogLine(LoggedLine(Topic::Players, "rauf connected")), "2026-10-02 10:59:06 [players] rauf connected");
}

TEST(LogFile, ReadsBackWhatItWrote)
{
    std::optional<Line> line = ParseLogLine("2026-10-02 10:59:06 [connection] [HTTP] Can't download: a.wav | HTTP Code: 404");
    ASSERT_TRUE(line.has_value());
    EXPECT_EQ(line->text, "[HTTP] Can't download: a.wav | HTTP Code: 404");
    EXPECT_EQ(line->topic, Topic::Connection);
    EXPECT_EQ(line->severity, Severity::Warning);
    EXPECT_EQ(line->time, SampleTime());
    EXPECT_TRUE(line->previous_session);
    ASSERT_EQ(line->segments.size(), 1u);
    EXPECT_TRUE(line->segments[0].themed);
}

TEST(LogFile, KeepsTheColorsOfColoredPieces)
{
    Line line = LoggedLine(Topic::Server, "");
    line.segments = { { kWhite, "[", true }, { { 0, 200, 0, 255 }, "JBE", false }, { kWhite, "] hi", true } };
    line.text = "[JBE] hi";

    std::string logged = FormatLogLine(line);
    EXPECT_EQ(logged, "2026-10-02 10:59:06 [server] [\x1B[38;2;0;200;0mJBE\x1B[0m] hi");

    std::optional<Line> parsed = ParseLogLine(logged);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->text, "[JBE] hi");
    ASSERT_EQ(parsed->segments.size(), 3u);
    EXPECT_TRUE(parsed->segments[0].themed);
    EXPECT_FALSE(parsed->segments[1].themed);
    EXPECT_EQ(parsed->segments[1].color, (Rgba{ 0, 200, 0, 255 }));
    EXPECT_EQ(parsed->segments[1].text, "JBE");
    EXPECT_TRUE(parsed->segments[2].themed);
}

TEST(LogFile, KeepsChatQuiet)
{
    std::optional<Line> line = ParseLogLine("2026-10-02 10:59:06 [chat] kotya : my game failed");
    ASSERT_TRUE(line.has_value());
    EXPECT_EQ(line->severity, Severity::Normal);
}

TEST(LogFile, SkipsLinesInAnotherFormat)
{
    EXPECT_FALSE(ParseLogLine("").has_value());
    EXPECT_FALSE(ParseLogLine("hello").has_value());
    EXPECT_FALSE(ParseLogLine("2026-10-02 10:59:06 rauf connected").has_value());
    EXPECT_FALSE(ParseLogLine("2026-10-02 10:59:06 [nonsense] rauf connected").has_value());
}

TEST(LogFile, FormatsTheClock)
{
    EXPECT_EQ(FormatClock(SampleTime()), "10:59:06");
}

TEST(ConsoleBufferLog, ReportsEveryFinishedLineOnce)
{
    ConsoleBuffer buffer;
    std::vector<std::string> closed;
    buffer.SetLineClosedHandler([&](const Line& line) { closed.push_back(line.text); });

    buffer.Print(kWhite, "one\ntw");
    buffer.Print(kWhite, "o\n\nthree");

    EXPECT_EQ(closed, (std::vector<std::string>{ "one", "two", "" }));
}

TEST(ConsoleBufferLog, KeepsTheWholeText)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "Blocked: ");
    buffer.Print({ 255, 0, 0, 255 }, "cmd\n");

    EXPECT_EQ(buffer.Lines()[0].text, "Blocked: cmd");
    EXPECT_NE(buffer.Lines()[0].time, 0);
}

TEST(ConsoleBufferLog, ShrinksToANewLimit)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "1\n2\n3\n");
    buffer.SetMaxLines(2);

    ASSERT_EQ(buffer.Lines().size(), 2u);
    EXPECT_EQ(buffer.Lines()[0].text, "2");
}

TEST(ConsoleBufferLog, PutsEarlierLinesInFront)
{
    ConsoleBuffer buffer(3);
    buffer.Print(kWhite, "now\n");
    buffer.AddEarlierLines({ LoggedLine(Topic::System, "old 1"), LoggedLine(Topic::System, "old 2"),
                             LoggedLine(Topic::System, "old 3") });

    // only two fit next to the line of this run, and the newest of them are the ones kept
    ASSERT_EQ(buffer.Lines().size(), 3u);
    EXPECT_EQ(buffer.Lines()[0].text, "old 2");
    EXPECT_EQ(buffer.Lines()[1].text, "old 3");
    EXPECT_EQ(buffer.Lines()[2].text, "now");
}

TEST(ConsoleBufferLog, AddsWholeLinesBeforeAnUnfinishedOne)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "typing");
    buffer.AddLine(LoggedLine(Topic::System, "restored"));
    buffer.Print(kWhite, " on\n");

    ASSERT_EQ(buffer.Lines().size(), 2u);
    EXPECT_EQ(buffer.Lines()[0].text, "restored");
    EXPECT_EQ(buffer.Lines()[1].text, "typing on");
}
