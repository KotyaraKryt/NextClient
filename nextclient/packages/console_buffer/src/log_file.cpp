#include <console_buffer/log_file.h>
#include <console_buffer/kinds.h>

#include <cstdio>
#include <ctime>

namespace console_buffer
{
    // the topics' names in the log, in the order of the Topic enum
    static const char* const kTopicTags[kTopicCount] = {
        "chat", "players", "server", "connection", "commands", "system", "debug",
    };

    static std::tm LocalTime(int64_t time)
    {
        std::time_t seconds = static_cast<std::time_t>(time);
        std::tm local = {};
#ifdef _WIN32
        localtime_s(&local, &seconds);
#else
        localtime_r(&seconds, &local);
#endif
        return local;
    }

    std::string FormatLogLine(const Line& line)
    {
        std::tm local = LocalTime(line.time);

        char stamp[32];
        std::snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d %02d:%02d:%02d",
            local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);

        std::string text = std::string(stamp) + " [" + kTopicTags[static_cast<int>(line.topic)] + "] ";
        for (const Segment& segment : line.segments)
        {
            if (segment.themed)
            {
                text += segment.text;
                continue;
            }

            char color[32];
            std::snprintf(color, sizeof(color), "\x1B[38;2;%d;%d;%dm", segment.color.r, segment.color.g, segment.color.b);
            text += color;
            text += segment.text;
            text += "\x1B[0m";
        }

        return text;
    }

    // the text after the tag, split at the color codes into themed and colored segments
    static void ParseSegments(std::string_view text, Line& line)
    {
        bool colored = false;
        Rgba color;

        while (!text.empty())
        {
            size_t escape = text.find('\x1B');
            std::string_view piece = text.substr(0, escape);
            if (!piece.empty())
            {
                line.segments.push_back({ colored ? color : Rgba{}, std::string(piece), !colored });
                line.text += piece;
            }

            if (escape == std::string_view::npos)
                break;

            text.remove_prefix(escape);
            size_t end = text.find('m');
            if (end == std::string_view::npos)
                break;

            std::string code(text.substr(0, end + 1));
            int r, g, b;
            if (std::sscanf(code.c_str(), "\x1B[38;2;%d;%d;%dm", &r, &g, &b) == 3)
            {
                colored = true;
                color = { static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), 255 };
            }
            else
            {
                colored = false;
            }

            text.remove_prefix(end + 1);
        }
    }

    std::optional<Line> ParseLogLine(std::string_view text)
    {
        std::tm local = {};
        int consumed = 0;
        std::string head(text.substr(0, 19));
        if (std::sscanf(head.c_str(), "%4d-%2d-%2d %2d:%2d:%2d%n", &local.tm_year, &local.tm_mon, &local.tm_mday,
                &local.tm_hour, &local.tm_min, &local.tm_sec, &consumed) != 6 || consumed != 19)
            return std::nullopt;

        local.tm_year -= 1900;
        local.tm_mon -= 1;
        local.tm_isdst = -1;

        // " [tag] " after the date
        std::string_view rest = text.substr(19);
        if (!rest.starts_with(" ["))
            return std::nullopt;

        size_t close = rest.find("] ");
        if (close == std::string_view::npos)
            return std::nullopt;

        std::string_view tag = rest.substr(2, close - 2);
        int topic = 0;
        while (topic < kTopicCount && tag != kTopicTags[topic])
            topic++;
        if (topic == kTopicCount)
            return std::nullopt;

        Line line;
        ParseSegments(rest.substr(close + 2), line);
        line.time = static_cast<int64_t>(std::mktime(&local));
        line.previous_session = true;
        line.topic = static_cast<Topic>(topic);

        // chat and debug output are never errors, whatever words they hold
        bool quiet = line.topic == Topic::Chat || line.topic == Topic::Developer;
        line.severity = quiet ? Severity::Normal : ClassifySeverity(Source::Normal, line.text);

        return line;
    }

    std::string FormatClock(int64_t time)
    {
        std::tm local = LocalTime(time);

        char clock[16];
        std::snprintf(clock, sizeof(clock), "%02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec);
        return clock;
    }
}
