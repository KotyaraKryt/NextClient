#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace console_buffer
{
    // The names that have the typed letters in the same order, not necessarily side by side:
    // "fpmx" finds "fps_max". Case doesn't matter. The names that start with the text come
    // first, then the ones that have it whole, then the rest; each group sorted, at most limit
    // in all. Same rules as the chat's slash command suggestions in experimental/chat-hud.
    std::vector<std::string> MatchNames(const std::vector<std::string>& names, std::string_view typed, size_t limit);
}
