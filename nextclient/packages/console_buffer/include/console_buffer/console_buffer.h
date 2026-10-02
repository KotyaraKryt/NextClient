#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
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
        // said by a player
        Chat,
        // shown in the chat but sent by the server: joins, team changes, radio, plugin notices
        ServerChat,
    };

    // What a line is about, for the filters; see Classify in kinds.h
    enum class Topic
    {
        Chat,
        Players,
        Server,
        Connection,
        Commands,
        System,
        Developer,
    };

    inline constexpr int kTopicCount = 7;

    // How much a line matters, for its color; a download can fail as much as a connection
    enum class Severity
    {
        Normal,
        Warning,
        Error,
    };

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
        // the segments' text joined, kept for searching and comparing lines
        std::string text;
        // seconds since the epoch when the line started
        int64_t time = 0;
        // brought back from console.log of an earlier run
        bool previous_session = false;
        // the mark between an earlier run's lines and this run's, shown whatever the filters
        bool divider = false;
        Source source = Source::Normal;
        Topic topic = Topic::System;
        Severity severity = Severity::Normal;
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

        // drops the oldest lines past the new limit right away
        void SetMaxLines(size_t max_lines);

        // called with every line that gets its line break, e.g. to write it to a log
        void SetLineClosedHandler(std::function<void(const Line&)> handler);

        // adds a whole line from elsewhere before an unfinished one
        void AddLine(Line line);

        // puts lines older than all the others in front, such as an earlier run's log;
        // the ones past the limit are left out
        void AddEarlierLines(std::vector<Line> lines);

        // drops what AddEarlierLines brought in, the earlier runs' lines and their divider
        void RemoveEarlierLines();

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
        std::function<void(const Line&)> on_line_closed_;
    };
}
