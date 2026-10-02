#pragma once

#include <console_buffer/console_buffer.h>

#include <string>
#include <string_view>

namespace console_buffer
{
    // What a line is about, from where it came from and the words it starts with or holds:
    // "Error: ..." is an error, "Couldn't open ..." a warning, "] cmd" a typed command
    Kind Classify(Source source, std::string_view text);

    // Lower case for ASCII and Russian letters in UTF-8; anything else is left as it is
    std::string ToLowerUtf8(std::string_view text);

    // needle_lower must already be lower case, so a search lowers it once, not once per line
    bool ContainsLowered(std::string_view text, std::string_view needle_lower);
}
