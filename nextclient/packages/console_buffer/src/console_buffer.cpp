#include <console_buffer/console_buffer.h>
#include <console_buffer/kinds.h>
#include <console_buffer/selection.h>

namespace console_buffer
{
    ConsoleBuffer::ConsoleBuffer(size_t max_lines) : max_lines_(max_lines)
    {
    }

    void ConsoleBuffer::Print(Rgba color, std::string_view text, bool themed, Source source)
    {
        generation_++;

        size_t start = 0;
        while (true)
        {
            size_t newline = text.find('\n', start);
            if (newline == std::string_view::npos)
            {
                AppendToLastLine(color, text.substr(start), themed, source);
                return;
            }

            AppendToLastLine(color, text.substr(start, newline - start), themed, source);

            if (!last_line_open_)
                StartLine(source);

            last_line_open_ = false;

            start = newline + 1;
        }
    }

    void ConsoleBuffer::Clear()
    {
        generation_++;
        lines_.clear();
        last_line_open_ = false;
    }

    void ConsoleBuffer::MarkNextLine(Source source)
    {
        next_source_ = source;
    }

    // Color codes (\x01-\x04 in GoldSrc chat) and other control bytes have no glyph and showed
    // up as "?"; a tab becomes spaces so that every character stays one column wide
    static std::string WithoutControlChars(std::string_view text)
    {
        std::string clean;
        clean.reserve(text.size());
        for (char ch : text)
        {
            if (ch == '\t')
                clean += "    ";
            else if (static_cast<unsigned char>(ch) >= 0x20 && ch != 0x7F)
                clean += ch;
        }

        return clean;
    }

    void ConsoleBuffer::AppendToLastLine(Rgba color, std::string_view raw_text, bool themed, Source source)
    {
        std::string text = WithoutControlChars(raw_text);
        if (text.empty())
            return;

        if (!last_line_open_)
            StartLine(source);

        Line& line = lines_.back();
        std::vector<Segment>& segments = line.segments;
        if (!segments.empty() && segments.back().color == color && segments.back().themed == themed)
            segments.back().text += text;
        else
            segments.push_back({ color, std::move(text), themed });

        // "Error" may arrive before the rest of its line, so the kind follows the whole text so far
        line.kind = Classify(line.source, LineText(line));
    }

    void ConsoleBuffer::StartLine(Source source)
    {
        lines_.emplace_back();
        if (lines_.size() > max_lines_)
            lines_.pop_front();

        Line& line = lines_.back();
        line.source = next_source_ != Source::Normal ? next_source_ : source;
        line.kind = Classify(line.source, "");
        next_source_ = Source::Normal;

        last_line_open_ = true;
    }
}
