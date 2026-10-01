#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <string_view>

namespace console_buffer
{
    // Commands typed into the console, oldest first, walked with Up and Down like a shell's
    class CommandHistory
    {
    public:
        explicit CommandHistory(size_t max_entries = 100);

        // Remembers a command and stops walking. An empty command or a repeat of the newest
        // one isn't added; past max_entries the oldest is forgotten.
        void Add(std::string_view command);

        // Up: one command older, staying on the oldest; nothing when there's no history
        std::optional<std::string> Older();
        // Down: one command newer, then "" for the empty input line; nothing when not walking
        std::optional<std::string> Newer();

        // one command per line, oldest first, for keeping the history between game runs
        std::string Save() const;
        void Load(std::string_view text);

        const std::deque<std::string>& Entries() const { return entries_; }

    private:
        std::deque<std::string> entries_;
        size_t max_entries_;
        // the entry Up and Down are on; entries_.size() is the empty line below the newest
        size_t cursor_ = 0;
    };
}
