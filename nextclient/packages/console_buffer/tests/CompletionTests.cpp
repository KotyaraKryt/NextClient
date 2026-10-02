#include <gtest/gtest.h>

#include <console_buffer/completion.h>

using console_buffer::MatchNames;
using Names = std::vector<std::string>;

static const Names kNames = { "cl_bob", "cl_cmdrate", "connect", "fps_max", "fps_override", "retry", "status" };

TEST(Completion, FindsLettersInOrder)
{
    EXPECT_EQ(MatchNames(kNames, "fpmx", 10), (Names{ "fps_max" }));
    EXPECT_EQ(MatchNames(kNames, "rty", 10), (Names{ "retry" }));
    EXPECT_TRUE(MatchNames(kNames, "xq", 10).empty());
    EXPECT_TRUE(MatchNames(kNames, "xamspf", 10).empty());
}

TEST(Completion, PutsNamesStartingWithTheTextFirst)
{
    // starts with it, then has it as a whole, then only has its letters in order
    EXPECT_EQ(MatchNames({ "status", "cl_status", "sxtatus" }, "status", 10), (Names{ "status", "cl_status", "sxtatus" }));
    EXPECT_EQ(MatchNames(kNames, "fps", 10), (Names{ "fps_max", "fps_override" }));
}

TEST(Completion, IgnoresCase)
{
    EXPECT_EQ(MatchNames({ "MP3Volume" }, "mp3vol", 10), (Names{ "MP3Volume" }));
    EXPECT_EQ(MatchNames(kNames, "FPS_MAX", 10), (Names{ "fps_max" }));
}

TEST(Completion, KeepsTheLimit)
{
    EXPECT_EQ(MatchNames(kNames, "c", 2).size(), 2u);
}
