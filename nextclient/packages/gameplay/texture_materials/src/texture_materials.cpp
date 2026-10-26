#include <texture_materials/texture_materials.h>

#include <cctype>

namespace
{
    bool IsBlank(char c)
    {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    }

    std::string MakeKey(std::string_view texture_name)
    {
        std::string key(texture_name.substr(0, texture_materials::kNameLength));
        for (char& c : key) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return key;
    }

    struct Entry
    {
        char material{};
        std::string_view texture_name;
    };

    std::optional<Entry> ParseLine(std::string_view line)
    {
        size_t i = 0;
        while (i < line.size() && IsBlank(line[i])) {
            i++;
        }

        // the stock parser treats a line that does not open with a letter as a comment
        if (i == line.size() || !std::isalpha(static_cast<unsigned char>(line[i]))) {
            return std::nullopt;
        }

        char material = static_cast<char>(std::toupper(static_cast<unsigned char>(line[i])));
        i++;

        while (i < line.size() && IsBlank(line[i])) {
            i++;
        }

        size_t name_start = i;
        while (i < line.size() && !IsBlank(line[i])) {
            i++;
        }

        if (i == name_start) {
            return std::nullopt;
        }

        return Entry{material, line.substr(name_start, i - name_start)};
    }
}

namespace texture_materials
{
    void MaterialTable::Load(std::string_view text)
    {
        size_t line_start = 0;
        while (line_start < text.size()) {
            size_t line_end = text.find('\n', line_start);
            if (line_end == std::string_view::npos) {
                line_end = text.size();
            }

            std::optional<Entry> entry = ParseLine(text.substr(line_start, line_end - line_start));
            if (entry) {
                materials_[MakeKey(entry->texture_name)] = entry->material;
            }

            line_start = line_end + 1;
        }
    }

    void MaterialTable::Clear()
    {
        materials_.clear();
    }

    std::optional<char> MaterialTable::Find(std::string_view texture_name) const
    {
        auto it = materials_.find(MakeKey(texture_name));
        if (it == materials_.end()) {
            return std::nullopt;
        }
        return it->second;
    }
}
