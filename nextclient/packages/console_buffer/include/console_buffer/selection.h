#pragma once

#include <console_buffer/console_buffer.h>

#include <compare>
#include <string>
#include <string_view>

// Selecting scrollback text with the mouse. Columns count characters, not bytes, and the
// console font is monospaced, so a column is always char_width pixels wide.
namespace console_buffer
{
    // A gap between characters: column 0 is before the first one, column N after the Nth
    struct TextPos
    {
        int line = 0;
        int column = 0;

        auto operator<=>(const TextPos&) const = default;
    };

    std::string LineText(const Line& line);

    // characters in UTF-8 text, and the byte where character number column starts
    int CharCount(std::string_view utf8);
    size_t ByteOffset(std::string_view utf8, int column);

    // The gap nearest to a point, with x and y measured from the top left corner of the
    // first line; points above, below or to the side land on the nearest line and column
    TextPos PositionAt(const ConsoleBuffer& buffer, float x, float y, float char_width, float line_height);

    // The text between two gaps in either order, lines joined with "\n"
    std::string SelectedText(const ConsoleBuffer& buffer, TextPos a, TextPos b);
}
