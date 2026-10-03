#pragma once

#include "ImGuiTheme.h"

#include <imgui/imgui.h>

#include <string>
#include <vector>

// How each of the ImGui windows looks: its colours, its background's opacity, its text size and, for
// the ones that dim the screen behind them, how much. Kept in archived cvars the options' Interface page
// edits; the colours are one palette for all the windows, which each can change some of for itself
namespace ImGuiAppearance
{
    enum class Element
    {
        Console,
        ServerBrowser,
        Options,
        CreateServer,
        PlayerList,
        Dialogs,
        Loading,
        DemoPlayer,
        Scoreboard,
        Motd,
        Count,
    };

    struct ElementInfo
    {
        const char* token;
        const char* english;
        // nullptr for a window whose opacity can't be seen
        const char* opacityCvar;
        const char* fontCvar;
        // nullptr for the windows that leave the screen behind them as it is
        const char* dimCvar;
        // the palette's colours this window has its own of
        const char* colorsCvar;
    };

    struct Values
    {
        float opacity;
        float fontSize;
        float dim;
        ThemePalette palette;
    };

    // some of a palette's colours, as a cvar keeps them: "accent=8e8923 text=d8ded3"
    struct PaletteOverrides
    {
        bool set[ThemePalette::Count] = {};
        ImVec4 colors[ThemePalette::Count] = {};
    };

    struct PalettePreset
    {
        const char* token;
        const char* english;
        ThemePalette palette;
    };

    // the cvar with the palette all the windows start from
    constexpr const char* kPaletteCvar = "ui_colors";

    constexpr float kMinOpacity = 0.3f;
    constexpr float kMinFontSize = 12.0f;
    constexpr float kMaxFontSize = 24.0f;
    constexpr float kDefaultFontSize = 16.0f;
    constexpr float kMaxDim = 0.9f;

    const ElementInfo& Info(Element element);
    // the default values of the element's cvars, as text for the cvar
    const char* DefaultValue(const char* cvar);

    // the cvars have to exist before config.cfg runs, or their saved values are lost
    void RegisterCvars();

    // the values the cvars have now, in their ranges
    Values Current(Element element);
    Values Clamp(Values values);

    PaletteOverrides ParseColors(const char* text);
    std::string FormatColors(const PaletteOverrides& overrides);
    // the default palette with the global cvar's colours over it, then the window's own
    ThemePalette ResolvePalette(const char* globalColors, const char* elementColors);

    const std::vector<PalettePreset>& Presets();
    // a palette colour's name for the Interface page, and its key in the cvars
    const char* ColorToken(ThemePalette::Color color);
    const char* ColorEnglish(ThemePalette::Color color);

    // the theme's background colours made see-through; base is the theme as it was applied
    void ApplyOpacity(ImGuiStyle& style, const ImVec4* base, float opacity);
}
