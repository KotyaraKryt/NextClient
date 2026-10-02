#include <console_buffer/selection.h>

#include <algorithm>
#include <cmath>

namespace console_buffer
{
    LineView AllLines(const ConsoleBuffer& buffer)
    {
        LineView lines;
        lines.reserve(buffer.Lines().size());
        for (const Line& line : buffer.Lines())
            lines.push_back(&line);

        return lines;
    }

    std::string LineText(const Line& line)
    {
        std::string text;
        for (const Segment& segment : line.segments)
            text += segment.text;

        return text;
    }

    // continuation bytes of a UTF-8 character look like 10xxxxxx
    static bool IsContinuationByte(char byte)
    {
        return (static_cast<unsigned char>(byte) & 0xC0) == 0x80;
    }

    int CharCount(std::string_view utf8)
    {
        int count = 0;
        for (char byte : utf8)
        {
            if (!IsContinuationByte(byte))
                count++;
        }

        return count;
    }

    size_t ByteOffset(std::string_view utf8, int column)
    {
        int chars = 0;
        for (size_t i = 0; i < utf8.size(); i++)
        {
            if (IsContinuationByte(utf8[i]))
                continue;

            if (chars == column)
                return i;
            chars++;
        }

        return utf8.size();
    }

    TextPos PositionAt(const LineView& lines, float x, float y, float char_width, float line_height)
    {
        int line_count = static_cast<int>(lines.size());
        if (line_count == 0)
            return {};

        TextPos pos;

        pos.line = static_cast<int>(std::floor(y / line_height));
        pos.line = std::clamp(pos.line, 0, line_count - 1);

        int length = CharCount(LineText(*lines[pos.line]));
        pos.column = static_cast<int>(std::round(x / char_width));
        pos.column = std::clamp(pos.column, 0, length);

        return pos;
    }

    std::string SelectedText(const LineView& lines, TextPos a, TextPos b)
    {
        TextPos from = std::min(a, b);
        TextPos to = std::max(a, b);

        // the oldest lines may have been dropped since the selection was made
        int last_line = static_cast<int>(lines.size()) - 1;
        to.line = std::min(to.line, last_line);

        std::string text;
        for (int i = from.line; i <= to.line; i++)
        {
            std::string line = LineText(*lines[i]);

            size_t start = i == from.line ? ByteOffset(line, from.column) : 0;
            size_t end = i == to.line ? ByteOffset(line, to.column) : line.size();
            text.append(line, start, end - start);

            if (i != to.line)
                text += '\n';
        }

        return text;
    }
}
