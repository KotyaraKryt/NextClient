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

    Kind Classify(Source source, std::string_view text)
    {
        if (source == Source::Chat)
            return Kind::Chat;
        if (source == Source::Developer)
            return Kind::Developer;

        if (text.starts_with("] "))
            return Kind::Command;

        // ConsoleCmdLogger's lines about commands the server wasn't allowed to run
        if (StartsWithAny(text, { "Blocked ", "(line breaks and tabulation" }))
            return Kind::Blocked;

        if (StartsWithAny(text, { "Error", "ERROR" }) || HasAny(text, { "FATAL", " error", "failed", "Failed" }))
            return Kind::Error;

        if (StartsWithAny(text, { "Warning", "WARNING" }) ||
            HasAny(text, { "Couldn't", "Can't", "Could not", "Cannot", "Unknown command" }))
            return Kind::Warning;

        return Kind::Info;
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
