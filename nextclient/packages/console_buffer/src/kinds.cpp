#include <console_buffer/kinds.h>

#include <initializer_list>

namespace console_buffer
{
    static bool HasAny(std::string_view text, std::initializer_list<std::string_view> words)
    {
        for (std::string_view word : words)
        {
            if (text.find(word) != std::string_view::npos)
                return true;
        }

        return false;
    }

    static bool StartsWithAny(std::string_view text, std::initializer_list<std::string_view> words)
    {
        for (std::string_view word : words)
        {
            if (text.starts_with(word))
                return true;
        }

        return false;
    }

    static bool EndsWithAny(std::string_view text, std::initializer_list<std::string_view> words)
    {
        while (!text.empty() && text.back() == ' ')
            text.remove_suffix(1);

        for (std::string_view word : words)
        {
            if (text.ends_with(word))
                return true;
        }

        return false;
    }

    // The rules follow the messages hw.so, client.so, cstrike/titles.txt and AMX Mod X print
    Topic ClassifyTopic(Source source, std::string_view text)
    {
        if (source == Source::Developer)
            return Topic::Developer;
        if (source == Source::Chat)
            return Topic::Chat;

        // typed by the player, or sent by the server and stopped by ConsoleCmdLogger
        if (text.starts_with("] ") || StartsWithAny(text, { "Blocked ", "(line breaks and tabulation" }))
            return Topic::Commands;

        if (HasAny(text, { " (RADIO): " }))
            return Topic::Chat;

        // titles.txt events and client.so's death notices; before Connection, which would
        // take "X has been idle for too long and has been kicked" for a kick of our own
        if (EndsWithAny(text, { " connected", " disconnected", " has left the game", " has joined the game", " dropped",
                                " has been kicked", " killed self", " died" }) ||
            HasAny(text, { " is joining the ", " killed ", " attacked a teammate", " changed name to ", "Teammate kills" }))
            return Topic::Players;

        if (StartsWithAny(text, { "Connecting to", "Connection accepted", "Commencing connection retry", "Retrying",
                                  "Redirecting connection", "NET Ports", "Server IP address", "Server refused connection",
                                  "Server protocol", "Server returned version", "Server is running game", "Dropped ",
                                  "Kicked", "Disconnected", "Lost connection", "[HTTP]", "Downloading", "Precaching",
                                  "Requesting", "Resources to request", "Aborting download", "Skipping", "Invalid file type" }) ||
            HasAny(text, { "timed out", "Timed out", "connection", "Reliable channel overflowed", "banned", "download",
                           "Download", "failed to transmit file", "not available from server", "missing from server",
                           "precache", "Precache" }))
            return Topic::Connection;

        if (StartsWithAny(text, { "BUILD ", "Server #", "Server cvar", "Server say", "Server logging", "Map name",
                                  "Loading map", "Time Remaining", "The game will restart", "Required number of votes",
                                  "Vote cast", "You voted", "* Privileges set" }) ||
            HasAny(text, { " vote)", " votes)" }))
            return Topic::Server;

        // whatever else the server put in the chat is its own announcement
        return source == Source::ServerChat ? Topic::Server : Topic::System;
    }

    Severity ClassifySeverity(Source source, std::string_view text)
    {
        // chat says whatever players type, and debug output is quiet on purpose
        if (source != Source::Normal)
            return Severity::Normal;

        if (StartsWithAny(text, { "Error", "ERROR", "Host_Error" }) ||
            HasAny(text, { "FATAL", " error", "failed", "Failed", "Refusing" }))
            return Severity::Error;

        if (StartsWithAny(text, { "Warning", "WARNING", "Blocked " }) ||
            HasAny(text, { "Couldn't", "Can't", "Could not", "Cannot", "Unknown command", "not found", "missing",
                           "Kicked", "kicked", "banned", "timed out", "Timed out", "refused" }))
            return Severity::Warning;

        return Severity::Normal;
    }

    void Classify(Line& line)
    {
        line.topic = ClassifyTopic(line.source, line.text);
        line.severity = ClassifySeverity(line.source, line.text);
    }

    std::string ToLowerUtf8(std::string_view text)
    {
        std::string lower(text);
        for (size_t i = 0; i < lower.size(); i++)
        {
            auto byte = static_cast<unsigned char>(lower[i]);
            if (byte >= 'A' && byte <= 'Z')
            {
                lower[i] = static_cast<char>(byte - 'A' + 'a');
                continue;
            }

            // continuation bytes are 80-BF, so they never pass for D0 or a Latin letter
            if (i + 1 >= lower.size())
                break;

            // Cyrillic capitals: Ё is D0 81, А-П D0 90-9F, Р-Я D0 A0-AF
            auto next = static_cast<unsigned char>(lower[i + 1]);
            if (byte == 0xD0 && next == 0x81)
            {
                lower[i] = static_cast<char>(0xD1);
                lower[i + 1] = static_cast<char>(0x91);
            }
            else if (byte == 0xD0 && next >= 0x90 && next <= 0x9F)
            {
                lower[i + 1] = static_cast<char>(next + 0x20);
            }
            else if (byte == 0xD0 && next >= 0xA0 && next <= 0xAF)
            {
                lower[i] = static_cast<char>(0xD1);
                lower[i + 1] = static_cast<char>(next - 0x20);
            }
        }

        return lower;
    }

    bool ContainsLowered(std::string_view text, std::string_view needle_lower)
    {
        return ToLowerUtf8(text).find(needle_lower) != std::string::npos;
    }
}
