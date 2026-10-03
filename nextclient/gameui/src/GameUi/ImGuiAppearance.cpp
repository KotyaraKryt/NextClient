#include "ImGuiAppearance.h"
#include "GameUi.h"

#include <cvardef.h>

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

namespace ImGuiAppearance
{
    namespace
    {
        // the console had its own opacity and text size before this page, which its gear button still sets;
        // the loading screen covers the whole screen, so it has no opacity to set
        const ElementInfo kElements[] = {
            { "#GameUI_AppearanceConsole", "Console", "con_opacity", "con_fontsize", nullptr, "ui_console_colors" },
            { "#GameUI_AppearanceBrowser", "Server browser", "ui_browser_alpha", "ui_browser_font", nullptr, "ui_browser_colors" },
            { "#GameUI_AppearanceOptions", "Options", "ui_options_alpha", "ui_options_font", nullptr, "ui_options_colors" },
            { "#GameUI_AppearanceCreateServer", "New game", "ui_newgame_alpha", "ui_newgame_font", nullptr, "ui_newgame_colors" },
            { "#GameUI_AppearancePlayerList", "Player list", "ui_plist_alpha", "ui_plist_font", nullptr, "ui_plist_colors" },
            { "#GameUI_AppearanceDialogs", "Questions and messages", "ui_dialogs_alpha", "ui_dialogs_font", "ui_dialogs_dim", "ui_dialogs_colors" },
            { "#GameUI_AppearanceLoading", "Loading screen", nullptr, "ui_loading_font", nullptr, "ui_loading_colors" },
            { "#GameUI_AppearanceDemoPlayer", "Demo player", "ui_demoui_alpha", "ui_demoui_font", nullptr, "ui_demoui_colors" },
            { "#GameUI_AppearanceScoreboard", "Scoreboard", "ui_scoreboard_alpha", "ui_scoreboard_font", nullptr, "ui_scoreboard_colors" },
            { "#GameUI_AppearanceMotd", "MOTD", "ui_motd_alpha", "ui_motd_font", "ui_motd_dim", "ui_motd_colors" },
        };
        static_assert(std::size(kElements) == static_cast<size_t>(Element::Count));

        struct Default
        {
            const char* cvar;
            const char* value;
        };

        // the scoreboard lets a little of the game through, as it always did
        const Default kDefaults[] = {
            { "ui_scoreboard_alpha", "0.95" },
            { "ui_dialogs_dim", "0.6" },
            { "ui_motd_dim", "0.6" },
        };

        const char* CvarString(const char* name)
        {
            cvar_t* cvar = name ? engine->pfnGetCvarPointer(name) : nullptr;
            return cvar && cvar->string ? cvar->string : "";
        }

        // the cvars' names for the palette's colours, in ThemePalette::Color's order
        const char* const kColorKeys[ThemePalette::Count] = { "window", "field", "button", "accent", "text", "border" };
        const char* const kColorTokens[ThemePalette::Count] = {
            "#GameUI_ColorWindow", "#GameUI_ColorField", "#GameUI_ColorButton", "#GameUI_ColorAccent", "#GameUI_ColorText", "#GameUI_ColorBorder",
        };
        const char* const kColorEnglish[ThemePalette::Count] = { "Window", "Fields and panels", "Buttons", "Accent", "Text", "Borders" };

        ImVec4 Rgb(int r, int g, int b)
        {
            return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
        }

        float CvarValue(const char* name, float fallback)
        {
            cvar_t* cvar = name ? engine->pfnGetCvarPointer(name) : nullptr;
            return cvar ? cvar->value : fallback;
        }
    }

    const ElementInfo& Info(Element element)
    {
        return kElements[static_cast<int>(element)];
    }

    const char* DefaultValue(const char* cvar)
    {
        for (const Default& value : kDefaults)
        {
            if (!strcmp(value.cvar, cvar))
                return value.value;
        }

        for (const ElementInfo& info : kElements)
        {
            if (info.opacityCvar && !strcmp(info.opacityCvar, cvar))
                return "1";
            if (!strcmp(info.fontCvar, cvar))
                return "16";
            if (!strcmp(info.colorsCvar, cvar))
                return "";
        }
        return "0";
    }

    void RegisterCvars()
    {
        engine->pfnRegisterVariable(const_cast<char*>(kPaletteCvar), const_cast<char*>(""), FCVAR_ARCHIVE);

        for (const ElementInfo& info : kElements)
        {
            // GameConsole registers the console's opacity and text size with its other settings
            bool console = &info == &Info(Element::Console);
            for (const char* cvar : { console ? nullptr : info.opacityCvar, console ? nullptr : info.fontCvar, info.dimCvar, info.colorsCvar })
            {
                if (cvar && !engine->pfnGetCvarPointer(cvar))
                    engine->pfnRegisterVariable(const_cast<char*>(cvar), const_cast<char*>(DefaultValue(cvar)), FCVAR_ARCHIVE);
            }
        }
    }

    Values Clamp(Values values)
    {
        values.opacity = std::clamp(values.opacity, kMinOpacity, 1.0f);
        values.fontSize = std::clamp(std::round(values.fontSize), kMinFontSize, kMaxFontSize);
        values.dim = std::clamp(values.dim, 0.0f, kMaxDim);
        return values;
    }

    Values Current(Element element)
    {
        const ElementInfo& info = Info(element);
        Values values;
        values.opacity = CvarValue(info.opacityCvar, 1.0f);
        values.fontSize = CvarValue(info.fontCvar, kDefaultFontSize);
        values.dim = info.dimCvar ? CvarValue(info.dimCvar, 0.6f) : 0.0f;
        values.palette = ResolvePalette(CvarString(kPaletteCvar), CvarString(info.colorsCvar));
        return Clamp(values);
    }

    void ApplyOpacity(ImGuiStyle& style, const ImVec4* base, float opacity)
    {
        // the backgrounds only: text, buttons and the lists' popups stay as solid as they were
        static const ImGuiCol kBackgrounds[] = {
            ImGuiCol_WindowBg, ImGuiCol_ChildBg, ImGuiCol_TitleBg, ImGuiCol_TitleBgActive, ImGuiCol_TitleBgCollapsed,
            ImGuiCol_MenuBarBg, ImGuiCol_TableHeaderBg, ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive,
        };

        for (ImGuiCol color : kBackgrounds)
        {
            style.Colors[color] = base[color];
            style.Colors[color].w *= opacity;
        }
    }

    PaletteOverrides ParseColors(const char* text)
    {
        PaletteOverrides overrides;
        std::string all = text ? text : "";
        size_t begin = 0;
        while (begin < all.size())
        {
            size_t end = all.find(' ', begin);
            if (end == std::string::npos)
                end = all.size();

            std::string entry = all.substr(begin, end - begin);
            size_t equals = entry.find('=');
            unsigned int rgb = 0;
            if (equals != std::string::npos && sscanf(entry.c_str() + equals + 1, "%6x", &rgb) == 1)
            {
                std::string key = entry.substr(0, equals);
                for (int i = 0; i < ThemePalette::Count; i++)
                {
                    if (key == kColorKeys[i])
                    {
                        overrides.set[i] = true;
                        overrides.colors[i] = Rgb((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
                    }
                }
            }
            begin = end + 1;
        }
        return overrides;
    }

    std::string FormatColors(const PaletteOverrides& overrides)
    {
        std::string text;
        for (int i = 0; i < ThemePalette::Count; i++)
        {
            if (!overrides.set[i])
                continue;

            const ImVec4& color = overrides.colors[i];
            char entry[32];
            snprintf(entry, sizeof(entry), "%s%s=%02x%02x%02x", text.empty() ? "" : " ", kColorKeys[i],
                static_cast<int>(color.x * 255.0f + 0.5f), static_cast<int>(color.y * 255.0f + 0.5f), static_cast<int>(color.z * 255.0f + 0.5f));
            text += entry;
        }
        return text;
    }

    ThemePalette ResolvePalette(const char* globalColors, const char* elementColors)
    {
        ThemePalette palette = DefaultPalette();
        for (const char* text : { globalColors, elementColors })
        {
            PaletteOverrides overrides = ParseColors(text);
            for (int i = 0; i < ThemePalette::Count; i++)
            {
                if (overrides.set[i])
                    palette.colors[i] = overrides.colors[i];
            }
        }
        return palette;
    }

    const std::vector<PalettePreset>& Presets()
    {
        // window, fields, buttons, accent, text, borders
        static const std::vector<PalettePreset> presets = {
            { "#GameUI_PaletteClassic", "Classic", DefaultPalette() },
            { "#GameUI_PaletteGraphite", "Graphite", { { Rgb(38, 40, 44), Rgb(28, 30, 33), Rgb(58, 62, 68), Rgb(232, 140, 40), Rgb(225, 228, 232), Rgb(18, 19, 21) } } },
            { "#GameUI_PaletteMidnight", "Midnight", { { Rgb(28, 34, 52), Rgb(20, 25, 40), Rgb(44, 54, 82), Rgb(74, 158, 255), Rgb(220, 228, 245), Rgb(12, 15, 26) } } },
            { "#GameUI_PaletteSand", "Sand", { { Rgb(112, 96, 72), Rgb(92, 78, 58), Rgb(134, 116, 88), Rgb(214, 160, 60), Rgb(240, 232, 216), Rgb(62, 52, 38) } } },
            { "#GameUI_PaletteCrimson", "Crimson", { { Rgb(48, 30, 32), Rgb(36, 22, 24), Rgb(78, 44, 48), Rgb(220, 60, 60), Rgb(236, 222, 222), Rgb(24, 14, 15) } } },
            { "#GameUI_PaletteLight", "Light", { { Rgb(222, 224, 218), Rgb(242, 243, 240), Rgb(196, 200, 190), Rgb(120, 150, 40), Rgb(30, 34, 28), Rgb(160, 164, 156) } } },
        };
        return presets;
    }

    const char* ColorToken(ThemePalette::Color color)
    {
        return kColorTokens[color];
    }

    const char* ColorEnglish(ThemePalette::Color color)
    {
        return kColorEnglish[color];
    }
}
