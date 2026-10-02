#include <console_buffer/completion.h>

#include <algorithm>
#include <utility>

namespace console_buffer
{
    static std::string ToLowerAscii(std::string_view text)
    {
        std::string lower(text);
        for (char& ch : lower)
        {
            if (ch >= 'A' && ch <= 'Z')
                ch = ch - 'A' + 'a';
        }

        return lower;
    }

    static bool HasLettersInOrder(std::string_view name, std::string_view letters)
    {
        size_t found = 0;
        for (char ch : name)
        {
            if (found < letters.size() && ch == letters[found])
                found++;
        }

        return found == letters.size();
    }

    std::vector<std::string> MatchNames(const std::vector<std::string>& names, std::string_view typed, size_t limit)
    {
        std::string wanted = ToLowerAscii(typed);

        // the group goes first in the pair, so sorting orders by group and then by name
        std::vector<std::pair<int, std::string>> matches;
        for (const std::string& name : names)
        {
            std::string lower = ToLowerAscii(name);
            if (lower.starts_with(wanted))
                matches.emplace_back(0, name);
            else if (lower.find(wanted) != std::string::npos)
                matches.emplace_back(1, name);
            else if (HasLettersInOrder(lower, wanted))
                matches.emplace_back(2, name);
        }

        std::sort(matches.begin(), matches.end());
        if (matches.size() > limit)
            matches.resize(limit);

        std::vector<std::string> result;
        for (auto& [group, name] : matches)
            result.push_back(std::move(name));

        return result;
    }
}
