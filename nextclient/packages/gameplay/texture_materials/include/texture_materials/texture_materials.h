#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace texture_materials
{
    // The stock client compares only this many characters of a texture name, ignoring case
    constexpr size_t kNameLength = 16;

    // Texture name to material letter
    class MaterialTable
    {
    public:
        // Takes the syntax of sound/materials.txt: "<letter> <texture>" lines, other lines skipped.
        // A later line for the same texture replaces an earlier one.
        void Load(std::string_view text);
        void Clear();

        std::optional<char> Find(std::string_view texture_name) const;

        // Keys are lower case and cut to kNameLength
        const std::unordered_map<std::string, char>& get_materials() const
        {
            return materials_;
        }

    private:
        std::unordered_map<std::string, char> materials_;
    };
}
