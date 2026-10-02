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

    // Where a line came from, when the text alone can't tell
    enum class Source
    {
        Normal,
        Developer,
        Chat,
    };

    // What a line is about, for styling and filtering; see Classify in kinds.h
    enum class Kind
    {
        Info,
        Warning,
        Error,
        Blocked,
        Chat,
        Command,
        Developer,
    };

    inline constexpr int kKindCount = 7;

    struct Segment
    {
        Rgba color;
        std::string text;
        // printed in the console's default color, which the line's kind may replace
        bool themed = false;
    };

    // The engine can change color in the middle of a line, so one line is several segments
    struct Line
    {
        std::vector<Segment> segments;
        Source source = Source::Normal;
        Kind kind = Kind::Info;
    };

    class ConsoleBuffer
    {
    public:
        explicit ConsoleBuffer(size_t max_lines = 5000);

        // Text comes in pieces that don't follow line breaks: "] echo hi" and its "\n"
        // can be two calls, and one call can hold several lines
        void Print(Rgba color, std::string_view text, bool themed = false, Source source = Source::Normal);
        void Clear();

        // The next line to start comes from source: the chat prints a line in many pieces,
        // and only the code printing it knows it's chat
        void MarkNextLine(Source source);

        const std::deque<Line>& Lines() const { return lines_; }

        // changes with every Print and Clear, so a view of the lines knows to rebuild
        uint64_t Generation() const { return generation_; }

    private:
        void AppendToLastLine(Rgba color, std::string_view text, bool themed, Source source);
        void StartLine(Source source);

        std::deque<Line> lines_;
        size_t max_lines_;
        bool last_line_open_ = false;
        Source next_source_ = Source::Normal;
        uint64_t generation_ = 0;
    };
}
