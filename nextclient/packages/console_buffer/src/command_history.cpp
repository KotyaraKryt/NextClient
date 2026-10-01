#include <console_buffer/command_history.h>

namespace console_buffer
{
    CommandHistory::CommandHistory(size_t max_entries) : max_entries_(max_entries)
    {
    }

    void CommandHistory::Add(std::string_view command)
    {
        if (!command.empty() && (entries_.empty() || entries_.back() != command))
        {
            entries_.emplace_back(command);
            if (entries_.size() > max_entries_)
                entries_.pop_front();
        }

        cursor_ = entries_.size();
    }

    std::optional<std::string> CommandHistory::Older()
    {
        if (entries_.empty())
            return std::nullopt;

        if (cursor_ > 0)
            cursor_--;

        return entries_[cursor_];
    }

    std::optional<std::string> CommandHistory::Newer()
    {
        if (cursor_ >= entries_.size())
            return std::nullopt;

        cursor_++;

        if (cursor_ == entries_.size())
            return std::string();

        return entries_[cursor_];
    }

    std::string CommandHistory::Save() const
    {
        std::string text;
        for (const std::string& entry : entries_)
        {
            text += entry;
            text += '\n';
        }

        return text;
    }

    void CommandHistory::Load(std::string_view text)
    {
        entries_.clear();
        cursor_ = 0;

        size_t start = 0;
        while (start < text.size())
        {
            size_t newline = text.find('\n', start);
            if (newline == std::string_view::npos)
                newline = text.size();

            std::string_view line = text.substr(start, newline - start);
            // a history saved on Windows can come back with \r\n line ends
            if (!line.empty() && line.back() == '\r')
                line.remove_suffix(1);

            Add(line);
            start = newline + 1;
        }
    }
}
