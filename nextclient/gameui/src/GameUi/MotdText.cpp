#include "MotdText.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

namespace
{
    // a link with more than this many is somebody's mistake, not a page
    constexpr size_t kMaxLinkLength = 1024;
    constexpr size_t kMaxLinks = 8;

    // Windows-1251's 0x80..0xBF; 0xC0..0xFF are А..я in a row
    const uint16_t kCp1251High[64] = {
        0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
        0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0xFFFD, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
        0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7, 0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
        0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
    };

    struct NamedEntity
    {
        const char* name;
        uint32_t codepoint;
    };

    const NamedEntity kEntities[] = {
        { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' }, { "apos", '\'' },
        { "copy", 0xA9 }, { "reg", 0xAE }, { "laquo", 0xAB }, { "raquo", 0xBB }, { "mdash", 0x2014 },
        { "ndash", 0x2013 }, { "hellip", 0x2026 }, { "bull", 0x2022 }, { "middot", 0xB7 }, { "trade", 0x2122 },
        { "deg", 0xB0 }, { "euro", 0x20AC }, { "times", 0xD7 },
    };

    void AppendUtf8(std::string& out, uint32_t cp)
    {
        if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            cp = 0xFFFD;

        if (cp < 0x80)
            out += static_cast<char>(cp);
        else if (cp < 0x800)
        {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool IsValidUtf8(const std::string& text)
    {
        size_t i = 0;
        while (i < text.size())
        {
            auto c = static_cast<unsigned char>(text[i]);
            int extra;
            if (c < 0x80)
                extra = 0;
            else if ((c & 0xE0) == 0xC0 && c >= 0xC2)
                extra = 1;
            else if ((c & 0xF0) == 0xE0)
                extra = 2;
            else if ((c & 0xF8) == 0xF0 && c <= 0xF4)
                extra = 3;
            else
                return false;

            for (int k = 1; k <= extra; k++)
            {
                if (i + k >= text.size() || (static_cast<unsigned char>(text[i + k]) & 0xC0) != 0x80)
                    return false;
            }
            i += extra + 1;
        }
        return true;
    }

    // Russian servers still write their MOTDs in the encoding Windows used for Cyrillic
    std::string FromCp1251(const std::string& text)
    {
        std::string out;
        out.reserve(text.size() * 2);
        for (char ch : text)
        {
            auto c = static_cast<unsigned char>(ch);
            if (c < 0x80)
                out += ch;
            else if (c < 0xC0)
                AppendUtf8(out, kCp1251High[c - 0x80]);
            else
                AppendUtf8(out, 0x0410 + (c - 0xC0));
        }
        return out;
    }

    std::string Lower(std::string text)
    {
        for (char& c : text)
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        return text;
    }

    bool IsSpace(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    }

    std::string Trim(const std::string& text)
    {
        size_t begin = 0;
        size_t end = text.size();
        while (begin < end && IsSpace(text[begin]))
            begin++;
        while (end > begin && IsSpace(text[end - 1]))
            end--;
        return text.substr(begin, end - begin);
    }

    // "<b", "</" or "<!" at text[i], and not "a < b"
    bool StartsTag(const std::string& text, size_t i)
    {
        if (text[i] != '<' || i + 1 >= text.size())
            return false;

        char next = text[i + 1];
        return isalpha(static_cast<unsigned char>(next)) || next == '/' || next == '!';
    }

    bool LooksLikeHtml(const std::string& text)
    {
        for (size_t i = 0; i < text.size(); i++)
        {
            if (StartsTag(text, i) && text.find('>', i) != std::string::npos)
                return true;
        }
        return false;
    }

    // what entity starts at text[i] ('&'), and how long it is; false if it's just an ampersand
    bool DecodeEntity(const std::string& text, size_t i, std::string& out, size_t& length)
    {
        size_t end = text.find(';', i);
        if (end == std::string::npos || end - i > 10)
            return false;

        std::string name = text.substr(i + 1, end - i - 1);
        uint32_t cp = 0;
        if (!name.empty() && name[0] == '#')
        {
            bool hex = name.size() > 1 && (name[1] == 'x' || name[1] == 'X');
            const char* digits = name.c_str() + (hex ? 2 : 1);
            if (!*digits)
                return false;
            char* stop = nullptr;
            cp = static_cast<uint32_t>(strtoul(digits, &stop, hex ? 16 : 10));
            if (*stop)
                return false;
        }
        else if (name == "nbsp")
            cp = ' ';
        else
        {
            std::string lower = Lower(name);
            for (const NamedEntity& entity : kEntities)
            {
                if (lower == entity.name)
                {
                    cp = entity.codepoint;
                    break;
                }
            }
            if (!cp)
                return false;
        }

        AppendUtf8(out, cp);
        length = end - i + 1;
        return true;
    }

    // an attribute's value from a tag's inside ("a href='...'"); lower is the same text lowercased
    std::string Attribute(const std::string& tag, const std::string& lower, const char* name)
    {
        std::string key = std::string(name) + "=";
        size_t at = 0;
        while ((at = lower.find(key, at)) != std::string::npos)
        {
            // "href=" and not "data-href="
            if (at > 0 && !IsSpace(lower[at - 1]))
            {
                at += key.size();
                continue;
            }

            size_t begin = at + key.size();
            if (begin >= tag.size())
                return "";

            char quote = tag[begin];
            if (quote == '"' || quote == '\'')
            {
                size_t end = tag.find(quote, begin + 1);
                return tag.substr(begin + 1, end == std::string::npos ? std::string::npos : end - begin - 1);
            }

            size_t end = begin;
            while (end < tag.size() && !IsSpace(tag[end]) && tag[end] != '>')
                end++;
            return tag.substr(begin, end - begin);
        }
        return "";
    }

    void AddLink(MotdText& doc, const std::string& link)
    {
        std::string trimmed = Trim(link);
        if (!IsWebLink(trimmed) || doc.links.size() >= kMaxLinks)
            return;
        if (std::find(doc.links.begin(), doc.links.end(), trimmed) == doc.links.end())
            doc.links.push_back(trimmed);
    }

    // collects the text into blocks as the tags open and close them
    class BlockBuilder
    {
    public:
        explicit BlockBuilder(MotdText& doc) : m_Doc(doc) {}

        void Text(char c, bool pre)
        {
            if (pre || !IsSpace(c))
            {
                m_Line += c;
                return;
            }

            if (!m_Line.empty() && m_Line.back() != ' ' && m_Line.back() != '\n')
                m_Line += ' ';
        }

        void Text(const std::string& text)
        {
            m_Line += text;
        }

        // a <td> after another one on the same row
        void Gap()
        {
            if (!m_Line.empty() && m_Line.back() != '\n')
            {
                TrimEnd();
                m_Line += "   ";
            }
        }

        void LineBreak()
        {
            TrimEnd();
            m_Line += '\n';
        }

        void Flush(MotdText::Kind next = MotdText::Kind::Paragraph)
        {
            std::string text = Trim(m_Line);
            if (!text.empty())
                m_Doc.blocks.push_back({ m_Kind, text });
            m_Line.clear();
            m_Kind = next;
        }

        void Rule()
        {
            Flush();
            if (!m_Doc.blocks.empty() && m_Doc.blocks.back().kind != MotdText::Kind::Rule)
                m_Doc.blocks.push_back({ MotdText::Kind::Rule, "" });
        }

    private:
        void TrimEnd()
        {
            while (!m_Line.empty() && m_Line.back() == ' ')
                m_Line.pop_back();
        }

        MotdText& m_Doc;
        std::string m_Line;
        MotdText::Kind m_Kind = MotdText::Kind::Paragraph;
    };

    bool IsBlockTag(const std::string& name)
    {
        static const char* const kTags[] = {
            "p", "div", "center", "table", "tr", "blockquote", "section", "article", "header", "footer",
            "main", "nav", "form", "ul", "ol", "dl", "dt", "dd", "pre", "body", "html", "address", "figure",
        };
        for (const char* tag : kTags)
        {
            if (name == tag)
                return true;
        }
        return false;
    }

    // tags whose insides aren't text to show
    bool IsSkippedTag(const std::string& name)
    {
        return name == "script" || name == "style" || name == "title" || name == "noscript" || name == "select"
            || name == "textarea" || name == "object" || name == "svg";
    }

    void ParseHtml(const std::string& html, MotdText& doc)
    {
        std::string lower = Lower(html);
        BlockBuilder builder(doc);
        int preDepth = 0;

        size_t i = 0;
        while (i < html.size())
        {
            char c = html[i];
            if (c == '&')
            {
                std::string decoded;
                size_t length = 0;
                if (DecodeEntity(html, i, decoded, length))
                {
                    builder.Text(decoded);
                    i += length;
                }
                else
                {
                    builder.Text(c, preDepth > 0);
                    i++;
                }
                continue;
            }

            if (c != '<' || !StartsTag(html, i))
            {
                builder.Text(c, preDepth > 0);
                i++;
                continue;
            }

            if (lower.compare(i, 4, "<!--") == 0)
            {
                size_t end = lower.find("-->", i + 4);
                i = end == std::string::npos ? html.size() : end + 3;
                continue;
            }

            // a tag left open at the very end still counts: MOTDs get cut off at the engine's limit
            size_t end = html.find('>', i);
            if (end == std::string::npos)
                end = html.size();

            std::string tag = html.substr(i + 1, end - i - 1);
            std::string tagLower = lower.substr(i + 1, end - i - 1);
            i = std::min(end + 1, html.size());

            bool closing = !tagLower.empty() && tagLower[0] == '/';
            size_t nameBegin = closing ? 1 : 0;
            size_t nameEnd = nameBegin;
            while (nameEnd < tagLower.size() && isalnum(static_cast<unsigned char>(tagLower[nameEnd])))
                nameEnd++;
            std::string name = tagLower.substr(nameBegin, nameEnd - nameBegin);

            if (!closing && IsSkippedTag(name))
            {
                size_t close = lower.find("</" + name, i);
                if (close == std::string::npos)
                    break;
                size_t closeEnd = html.find('>', close);
                i = closeEnd == std::string::npos ? html.size() : closeEnd + 1;
                continue;
            }

            if (name == "br")
                builder.LineBreak();
            else if (name == "hr")
                builder.Rule();
            else if (name.size() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6')
                builder.Flush(closing ? MotdText::Kind::Paragraph : MotdText::Kind::Heading);
            else if (name == "li")
                builder.Flush(closing ? MotdText::Kind::Paragraph : MotdText::Kind::ListItem);
            else if (name == "td" || name == "th")
            {
                if (!closing)
                    builder.Gap();
            }
            else if (IsBlockTag(name))
            {
                builder.Flush();
                if (name == "pre")
                    preDepth = std::max(0, preDepth + (closing ? -1 : 1));
            }
            else if (!closing && name == "a")
                AddLink(doc, Attribute(tag, tagLower, "href"));
            else if (!closing && (name == "iframe" || name == "frame" || name == "embed"))
            {
                std::string src = Trim(Attribute(tag, tagLower, "src"));
                if (doc.url.empty() && IsWebLink(src))
                    doc.url = src;
            }
            else if (!closing && name == "meta" && Lower(Attribute(tag, tagLower, "http-equiv")) == "refresh")
            {
                // content="0; url=http://..."
                std::string content = Attribute(tag, tagLower, "content");
                size_t at = Lower(content).find("url=");
                if (at != std::string::npos && doc.url.empty())
                {
                    std::string target = Trim(content.substr(at + 4));
                    if (!target.empty() && (target[0] == '\'' || target[0] == '"'))
                        target = target.substr(1, target.find(target[0], 1) - 1);
                    if (IsWebLink(target))
                        doc.url = target;
                }
            }
        }

        builder.Flush();

        while (!doc.blocks.empty() && doc.blocks.back().kind == MotdText::Kind::Rule)
            doc.blocks.pop_back();
    }

    // plain text: lines next to each other make a paragraph, blank lines part them
    void ParsePlain(const std::string& text, MotdText& doc)
    {
        std::string paragraph;
        size_t begin = 0;
        while (begin <= text.size())
        {
            size_t end = text.find('\n', begin);
            if (end == std::string::npos)
                end = text.size();

            std::string line = text.substr(begin, end - begin);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (Trim(line).empty())
            {
                if (!paragraph.empty())
                    doc.blocks.push_back({ MotdText::Kind::Paragraph, paragraph });
                paragraph.clear();
            }
            else
            {
                if (!paragraph.empty())
                    paragraph += '\n';
                paragraph += line;
            }

            begin = end + 1;
        }

        if (!paragraph.empty())
            doc.blocks.push_back({ MotdText::Kind::Paragraph, paragraph });
    }
}

bool IsWebLink(const std::string& text)
{
    if (text.empty() || text.size() > kMaxLinkLength)
        return false;

    std::string lower = Lower(text.substr(0, 8));
    if (lower.rfind("http://", 0) != 0 && lower.rfind("https://", 0) != 0)
        return false;

    for (char c : text)
    {
        if (static_cast<unsigned char>(c) <= ' ' || c == '"' || c == '\'' || c == '<' || c == '>')
            return false;
    }
    return true;
}

MotdText ParseMotd(const std::string& raw)
{
    MotdText doc;
    std::string text = IsValidUtf8(raw) ? raw : FromCp1251(raw);

    std::string trimmed = Trim(text);
    if (IsWebLink(trimmed))
    {
        doc.url = trimmed;
        return doc;
    }

    doc.html = LooksLikeHtml(text);
    if (doc.html)
        ParseHtml(text, doc);
    else
        ParsePlain(text, doc);

    // the page's own link is a button already
    doc.links.erase(std::remove(doc.links.begin(), doc.links.end(), doc.url), doc.links.end());
    return doc;
}
