#pragma once

#include <console_buffer/console_buffer.h>

#include <string>
#include <string_view>

namespace console_buffer
{
    // What a line is about and how much it matters, from where it came from and the words
    // the engine, CS and AMX Mod X put in their messages: "Connecting to ..." is network,
    // "Error: server failed to transmit file ..." a download error, "] cmd" a typed command
    Topic ClassifyTopic(Source source, std::string_view text);
    Severity ClassifySeverity(Source source, std::string_view text);

    // fills line.topic and line.severity from its source and text
    void Classify(Line& line);

    // Lower case for ASCII and Russian letters in UTF-8; anything else is left as it is
    std::string ToLowerUtf8(std::string_view text);

    // needle_lower must already be lower case, so a search lowers it once, not once per line
    bool ContainsLowered(std::string_view text, std::string_view needle_lower);
}
