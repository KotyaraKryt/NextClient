#pragma once

#include <string>
#include <vector>

// What's left of a server's MOTD once its HTML is gone: ImGui can draw text, not web pages
struct MotdText
{
    enum class Kind
    {
        Heading,
        Paragraph,
        ListItem,
        Rule,
    };

    struct Block
    {
        Kind kind;
        // UTF-8, '\n' where the page had <br>
        std::string text;
    };

    std::vector<Block> blocks;
    // the page the MOTD sends to (the whole MOTD being a link, a refresh or a frame), "" if none
    std::string url;
    // the http(s) links in the page, in order and without repeats
    std::vector<std::string> links;
    bool html = false;
};

// raw is what the server sent; it's turned to UTF-8 from Windows-1251 when it isn't UTF-8 already
MotdText ParseMotd(const std::string& raw);

bool IsWebLink(const std::string& text);
