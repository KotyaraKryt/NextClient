#pragma once

#include <console_buffer/console_buffer.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// console.log: one readable line per console line, which a later run can bring back
//   2026-10-02 10:59:06 [players] rauf connected
// Pieces in colors of their own, like a server's chat notices, are wrapped in the 24-bit color
// codes of terminals (ESC[38;2;r;g;bm ... ESC[0m), so `less -R console.log` shows them too;
// the console strips control bytes from its text, so an ESC in the log is always one of these.
namespace console_buffer
{
    std::string FormatLogLine(const Line& line);

    // The line as it was logged, marked as coming from an earlier run; nothing for a line
    // that isn't in the format
    std::optional<Line> ParseLogLine(std::string_view text);

    // "10:59:06" in local time, for the console's timestamps
    std::string FormatClock(int64_t time);
}
