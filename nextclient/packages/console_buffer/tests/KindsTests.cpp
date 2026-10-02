#include <gtest/gtest.h>

#include <console_buffer/kinds.h>

using namespace console_buffer;

static const Rgba kWhite = { 255, 255, 255, 255 };

static Topic TopicOf(std::string_view text)
{
    return ClassifyTopic(Source::Normal, text);
}

static Severity SeverityOf(std::string_view text)
{
    return ClassifySeverity(Source::Normal, text);
}

// lines from a real session on a public server, and the formats hw.so, client.so and
// cstrike/titles.txt print
TEST(ClassifyTopic, PutsConnectionsAndDownloadsUnderConnection)
{
    EXPECT_EQ(TopicOf("NET Ports:  server 27015, client 27005"), Topic::Connection);
    EXPECT_EQ(TopicOf("Server IP address 192.168.1.221:27015"), Topic::Connection);
    EXPECT_EQ(TopicOf("Connecting to 46.174.53.199:27015..."), Topic::Connection);
    EXPECT_EQ(TopicOf("Connection accepted by 46.174.53.199:27015"), Topic::Connection);
    EXPECT_EQ(TopicOf("Commencing connection retry to 46.174.53.199:27015"), Topic::Connection);
    EXPECT_EQ(TopicOf("Kicked by Console: AFK"), Topic::Connection);
    EXPECT_EQ(TopicOf("Dropped kotya from server"), Topic::Connection);
    EXPECT_EQ(TopicOf("[HTTP] Start downloading from: http://fastdl.myarena.ru/14-131185/"), Topic::Connection);
    EXPECT_EQ(TopicOf("[HTTP] Can't download: sound/vehicles/airhorn1.wav | HTTP Code: 404"), Topic::Connection);
    EXPECT_EQ(TopicOf("Error: server failed to transmit file 'sound/vehicles/diesel_start1.wav'"), Topic::Connection);
    EXPECT_EQ(TopicOf("Downloading maps/de_dust2.bsp"), Topic::Connection);
}

TEST(ClassifyTopic, PutsWhatPlayersDoUnderPlayers)
{
    EXPECT_EQ(TopicOf("rauf connected"), Topic::Players);
    EXPECT_EQ(TopicOf("YourCS | User dropped"), Topic::Players);
    EXPECT_EQ(TopicOf("pera bezburgijas has left the game"), Topic::Players);
    EXPECT_EQ(TopicOf("zzzzzz disconnected"), Topic::Players);
    EXPECT_EQ(TopicOf("kotya is joining the Counter-Terrorist force (auto)"), Topic::Players);
    EXPECT_EQ(TopicOf("kotya has been idle for too long and has been kicked"), Topic::Players);
    EXPECT_EQ(TopicOf("kotya attacked a teammate"), Topic::Players);
    EXPECT_EQ(TopicOf("*** pera killed ZHANIK with a headshot from ak47 ***"), Topic::Players);
    EXPECT_EQ(TopicOf("pera killed his teammate ZHANIK with m4a1"), Topic::Players);
    EXPECT_EQ(TopicOf("ZHANIK killed self"), Topic::Players);
    EXPECT_EQ(TopicOf("ZHANIK died"), Topic::Players);
    EXPECT_EQ(TopicOf("* Player changed name to kotya"), Topic::Players);
}

TEST(ClassifyTopic, PutsTheServerAndItsAnnouncementsUnderServer)
{
    EXPECT_EQ(TopicOf("BUILD 3911 SERVER (0 CRC)"), Topic::Server);
    EXPECT_EQ(TopicOf("Server # 5"), Topic::Server);
    EXPECT_EQ(TopicOf("* Privileges set"), Topic::Server);
    EXPECT_EQ(TopicOf("Loading map \"de_dust2\""), Topic::Server);
    EXPECT_EQ(TopicOf("Time Remaining:  12:30"), Topic::Server);
    EXPECT_EQ(TopicOf("The game will restart in 3 SECONDS"), Topic::Server);
    EXPECT_EQ(TopicOf("de_dust2 :  3 (3 votes)"), Topic::Server);
    EXPECT_EQ(ClassifyTopic(Source::ServerChat, "[JBE] Добро пожаловать, kotya!"), Topic::Server);
}

TEST(ClassifyTopic, ReadsServerChatLinesByTheirText)
{
    EXPECT_EQ(ClassifyTopic(Source::ServerChat, "kotya is joining the Terrorist force"), Topic::Players);
    EXPECT_EQ(ClassifyTopic(Source::ServerChat, "kotya (RADIO): Fire in the hole!"), Topic::Chat);
}

TEST(ClassifyTopic, PutsTypedAndBlockedCommandsUnderCommands)
{
    EXPECT_EQ(TopicOf("] fps_max 100"), Topic::Commands);
    EXPECT_EQ(TopicOf("Blocked stufftext cmd (by engine): fullserverinfo"), Topic::Commands);
    EXPECT_EQ(TopicOf("(line breaks and tabulation are displayed as \\n and \\t, respectively)"), Topic::Commands);
}

TEST(ClassifyTopic, TrustsTheSourceOverTheText)
{
    EXPECT_EQ(ClassifyTopic(Source::Chat, "kotya connected"), Topic::Chat);
    EXPECT_EQ(ClassifyTopic(Source::Developer, "Connecting to nowhere"), Topic::Developer);
}

TEST(ClassifyTopic, LeavesTheRestToSystem)
{
    EXPECT_EQ(TopicOf("Couldn't open file overviews/jail_especial_v2.txt. Using default values for overiew mode."), Topic::System);
    EXPECT_EQ(TopicOf("Unknown command: fullserverinfo"), Topic::System);
    EXPECT_EQ(TopicOf(""), Topic::System);
}

TEST(ClassifySeverity, FindsErrorsAndWarningsInAnyTopic)
{
    EXPECT_EQ(SeverityOf("Error: server failed to transmit file 'sound/x.wav'"), Severity::Error);
    EXPECT_EQ(SeverityOf("Connection failed after 4 retries"), Severity::Error);
    EXPECT_EQ(SeverityOf("[HTTP] Can't download: sound/vehicles/airhorn1.wav | HTTP Code: 404"), Severity::Warning);
    EXPECT_EQ(SeverityOf("Couldn't open file overviews/jb_mini.txt"), Severity::Warning);
    EXPECT_EQ(SeverityOf("Unknown command: fullserverinfo"), Severity::Warning);
    EXPECT_EQ(SeverityOf("Blocked stufftext cmd (by engine): fullserverinfo"), Severity::Warning);
    EXPECT_EQ(SeverityOf("Connecting to 46.174.53.199:27015..."), Severity::Normal);
}

TEST(ClassifySeverity, LeavesChatAndDebugAlone)
{
    EXPECT_EQ(ClassifySeverity(Source::Chat, "this game failed me"), Severity::Normal);
    EXPECT_EQ(ClassifySeverity(Source::Developer, "Error: missing sprite"), Severity::Normal);
}

TEST(ConsoleBufferKinds, ClassifiesTheWholeLineAsItArrives)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "Err");
    EXPECT_EQ(buffer.Lines()[0].severity, Severity::Normal);

    buffer.Print(kWhite, "or: something\n");
    EXPECT_EQ(buffer.Lines()[0].severity, Severity::Error);
}

TEST(ConsoleBufferKinds, MarksTheNextLineAsChat)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "before\n");
    buffer.MarkNextLine(Source::Chat);
    buffer.Print(kWhite, "kotya");
    buffer.Print(kWhite, ": hi\n");
    buffer.Print(kWhite, "after\n");

    EXPECT_EQ(buffer.Lines()[0].topic, Topic::System);
    EXPECT_EQ(buffer.Lines()[1].topic, Topic::Chat);
    EXPECT_EQ(buffer.Lines()[2].topic, Topic::System);
}

TEST(ConsoleBufferKinds, KeepsDeveloperTextApart)
{
    ConsoleBuffer buffer;
    buffer.Print(kWhite, "texture load\n", false, Source::Developer);

    EXPECT_EQ(buffer.Lines()[0].topic, Topic::Developer);
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
