#pragma once

#include <console_buffer/console_buffer.h>

#include <compare>
#include <string>
#include <string_view>
#include <vector>

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

    // The lines on screen: all of them, or only the ones a filter lets through
    using LineView = std::vector<const Line*>;

    LineView AllLines(const ConsoleBuffer& buffer);

    std::string LineText(const Line& line);

    // characters in UTF-8 text, and the byte where character number column starts
    int CharCount(std::string_view utf8);
    size_t ByteOffset(std::string_view utf8, int column);

    // The gap nearest to a point, with x and y measured from the top left corner of the
    // first line; points above, below or to the side land on the nearest line and column
    TextPos PositionAt(const LineView& lines, float x, float y, float char_width, float line_height);

    // The text between two gaps in either order, lines joined with "\n"
    std::string SelectedText(const LineView& lines, TextPos a, TextPos b);

    // One row on screen: characters [start, end) of a line of the view, which a line too
    // long for the window is split into
    struct Row
    {
        int line = 0;
        int start = 0;
        int end = 0;

        bool operator==(const Row&) const = default;
    };

    // Splits text into pieces of at most columns characters, after the last space that
    // fits, or right at the edge for a word longer than a whole row. The spaces stay at the
    // end of their row, so the pieces put together are the text again. Empty text is one
    // empty piece.
    std::vector<Row> WrapText(std::string_view utf8, int columns);

    // every line of the view as its rows, in order; columns <= 0 keeps each line whole
    std::vector<Row> WrapLines(const LineView& lines, int columns);

    // Like PositionAt, over rows: y picks the row, x the gap in it, given in its line's columns
    TextPos PositionAtRow(const LineView& lines, const std::vector<Row>& rows, float x, float y, float char_width, float line_height);
}
