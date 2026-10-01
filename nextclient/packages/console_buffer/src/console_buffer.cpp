#include <console_buffer/console_buffer.h>

namespace console_buffer
{
    ConsoleBuffer::ConsoleBuffer(size_t max_lines) : max_lines_(max_lines)
    {
    }

    void ConsoleBuffer::Print(Rgba color, std::string_view text)
    {
        size_t start = 0;
        while (true)
        {
            size_t newline = text.find('\n', start);
            if (newline == std::string_view::npos)
            {
                AppendToLastLine(color, text.substr(start));
                return;
            }

            AppendToLastLine(color, text.substr(start, newline - start));

            if (!last_line_open_)
                StartLine();

            last_line_open_ = false;

            start = newline + 1;
        }
    }

    void ConsoleBuffer::Clear()
    {
        lines_.clear();
        last_line_open_ = false;
    }

    void ConsoleBuffer::AppendToLastLine(Rgba color, std::string_view text)
    {
        if (text.empty())
            return;

        if (!last_line_open_)
            StartLine();

        std::vector<Segment>& segments = lines_.back().segments;
        if (!segments.empty() && segments.back().color == color)
            segments.back().text += text;
        else
            segments.push_back({ color, std::string(text) });
    }

    void ConsoleBuffer::StartLine()
    {
        lines_.emplace_back();
        if (lines_.size() > max_lines_)
            lines_.pop_front();

        last_line_open_ = true;
    }
}
