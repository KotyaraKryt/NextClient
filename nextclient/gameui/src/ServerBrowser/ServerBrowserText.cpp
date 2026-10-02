#include "ServerBrowserText.h"

#include <algorithm>
#include <cwchar>

#include <nitro_utils/string_utils.h>
#include <strtools.h>

namespace
{
    constexpr std::string_view kUtf8Bom = "\xEF\xBB\xBF";
    constexpr std::string_view kCommentStart = "//";
    constexpr std::string_view kBlankChars = " \t\r";
    constexpr char kNativeNameSeparator = ';';

    std::wstring LowerCaseUtf8(std::string_view utf8)
    {
        return nitro_utils::to_lower_copy(nitro_utils::utf8_to_wide(utf8));
    }

    bool IsRightToLeft(wchar_t c)
    {
        // Hebrew, and its presentation forms
        return (c >= 0x0590 && c <= 0x05FF) || (c >= 0xFB1D && c <= 0xFB4F);
    }

    bool IsDigit(wchar_t c)
    {
        return c >= L'0' && c <= L'9';
    }

    // what can stand between two Hebrew words and still be read with them
    bool JoinsRightToLeftRun(wchar_t c)
    {
        return IsRightToLeft(c) || IsDigit(c) || (c != L'\0' && wcschr(L" -.,:;!?'\"()[]{}<>/", c));
    }

    // a separator inside a number, which keeps it one number: 24/7, 1.5, 12:00
    bool IsNumberSeparator(wchar_t c)
    {
        return c == L'.' || c == L',' || c == L'/' || c == L':';
    }

    wchar_t Mirrored(wchar_t c)
    {
        switch (c)
        {
            case L'(': return L')';
            case L')': return L'(';
            case L'[': return L']';
            case L']': return L'[';
            case L'{': return L'}';
            case L'}': return L'{';
            case L'<': return L'>';
            case L'>': return L'<';
            default: return c;
        }
    }
} // namespace

int ServerBrowserText_CompareUnknownLast(const char* v1, const char* v2)
{
    if (!v1[0] || !v2[0])
    {
        return (v1[0] ? 0 : 1) - (v2[0] ? 0 : 1);
    }

    return V_stricmp(v1, v2);
}

int ServerBrowserText_CompareUnknownLast(const wchar_t* v1, const wchar_t* v2)
{
    if (!v1[0] || !v2[0])
    {
        return (v1[0] ? 0 : 1) - (v2[0] ? 0 : 1);
    }

    return _wcsicmp(v1, v2);
}

std::string ServerBrowserText_ToVisualOrder(std::string_view utf8)
{
    std::wstring text = nitro_utils::utf8_to_wide(utf8);
    if (std::none_of(text.begin(), text.end(), IsRightToLeft))
    {
        return std::string(utf8);
    }

    size_t start = 0;
    while (start < text.size())
    {
        if (!IsRightToLeft(text[start]))
        {
            start++;
            continue;
        }

        // the run ends at its last Hebrew letter, what follows it reads left to right again
        size_t end = start + 1;
        for (size_t i = start; i < text.size() && JoinsRightToLeftRun(text[i]); i++)
        {
            if (IsRightToLeft(text[i]))
            {
                end = i + 1;
            }
        }

        std::reverse(text.begin() + start, text.begin() + end);
        std::transform(text.begin() + start, text.begin() + end, text.begin() + start, Mirrored);

        // the reversal turned the numbers around too
        for (size_t i = start; i < end;)
        {
            if (!IsDigit(text[i]))
            {
                i++;
                continue;
            }

            size_t number_end = i;
            while (number_end < end &&
                   (IsDigit(text[number_end]) || (IsNumberSeparator(text[number_end]) && number_end + 1 < end && IsDigit(text[number_end + 1]))))
            {
                number_end++;
            }

            std::reverse(text.begin() + i, text.begin() + number_end);
            i = number_end;
        }

        start = end;
    }

    return nitro_utils::wide_to_utf8(text);
}

std::string ServerBrowserText_GetCountryLabel(const std::string& code, const std::string& name)
{
    return name.empty() ? code : name;
}

CountryNativeNames ServerBrowserText_ParseCountryNativeNames(std::string_view text)
{
    CountryNativeNames native_names;

    if (text.starts_with(kUtf8Bom))
    {
        text.remove_prefix(kUtf8Bom.size());
    }

    while (!text.empty())
    {
        size_t line_end = text.find('\n');
        std::string_view line = nitro_utils::trim_view(text.substr(0, line_end), kBlankChars);
        text.remove_prefix(line_end == std::string_view::npos ? text.size() : line_end + 1);

        bool has_code = line.size() > kCountryCodeLength && nitro_utils::is_alpha_ascii(line[0]) &&
                        nitro_utils::is_alpha_ascii(line[1]) &&
                        (line[kCountryCodeLength] == ' ' || line[kCountryCodeLength] == '\t');

        if (line.starts_with(kCommentStart) || !has_code)
        {
            continue;
        }

        std::string code(line.substr(0, kCountryCodeLength));
        nitro_utils::to_upper(code);

        std::string_view names = line.substr(kCountryCodeLength);

        while (!names.empty())
        {
            size_t separator = names.find(kNativeNameSeparator);
            std::string_view name = nitro_utils::trim_view(names.substr(0, separator), kBlankChars);
            names.remove_prefix(separator == std::string_view::npos ? names.size() : separator + 1);

            if (!name.empty())
            {
                native_names.by_code[code].push_back(LowerCaseUtf8(name));
            }
        }
    }

    return native_names;
}

std::string ServerBrowserText_FindCountryCode(
    const std::map<std::string, std::string>& countries,
    const CountryNativeNames& native_names,
    const wchar_t* lower_text
)
{
    if (!lower_text[0])
    {
        return {};
    }

    for (const auto& [code, name] : countries)
    {
        if (LowerCaseUtf8(ServerBrowserText_GetCountryLabel(code, name)) == lower_text)
        {
            return code;
        }

        auto native_it = native_names.by_code.find(code);

        if (native_it != native_names.by_code.end() &&
            std::ranges::find(native_it->second, std::wstring_view(lower_text)) != native_it->second.end())
        {
            return code;
        }
    }

    return {};
}

bool ServerBrowserText_MatchesCountryFilter(
    const ServerDetailsNext& details,
    const CountryNativeNames& native_names,
    const char* code_filter,
    const wchar_t* lower_text
)
{
    if (code_filter[0])
    {
        return !V_stricmp(details.country_code, code_filter);
    }

    if (!details.country_code[0])
    {
        return false;
    }

    if (LowerCaseUtf8(details.country_code) == lower_text)
    {
        return true;
    }

    std::wstring name = LowerCaseUtf8(details.country_name);

    if (name.starts_with(lower_text))
    {
        return true;
    }

    auto native_it = native_names.by_code.find(details.country_code);

    return native_it != native_names.by_code.end() && std::ranges::any_of(native_it->second, [lower_text](const std::wstring& native_name) {
               return native_name.starts_with(lower_text);
           });
}
