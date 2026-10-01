#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

// The console's scrollback: what the engine prints, split into lines of colored pieces.
// Nothing here depends on the engine or on how the lines are drawn.
namespace console_buffer
{
    struct Rgba
    {
        uint8_t r = 255;
        uint8_t g = 255;
        uint8_t b = 255;
        uint8_t a = 255;

        bool operator==(const Rgba&) const = default;
    };

    struct Segment
    {
        Rgba color;
        std::string text;
    };

    // The engine can change color in the middle of a line, so one line is several segments
    struct Line
    {
        std::vector<Segment> segments;
    };

    class ConsoleBuffer
    {
    public:
        explicit ConsoleBuffer(size_t max_lines = 5000);

        // Text comes in pieces that don't follow line breaks: "] echo hi" and its "\n"
        // can be two calls, and one call can hold several lines
        void Print(Rgba color, std::string_view text);
        void Clear();

        const std::deque<Line>& Lines() const { return lines_; }

    private:
        void AppendToLastLine(Rgba color, std::string_view text);
        void StartLine();

        std::deque<Line> lines_;
        size_t max_lines_;
        bool last_line_open_ = false;
    };
}
