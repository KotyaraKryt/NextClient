#pragma once

#include <imgui/imgui.h>

// How each of the ImGui windows looks: its background's opacity, its text size and, for the ones
// that dim the screen behind them, how much. Kept in archived cvars the options' Interface page edits
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
    };

    struct Values
    {
        float opacity;
        float fontSize;
        float dim;
    };

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

    // the theme's background colours made see-through; base is the theme as it was applied
    void ApplyOpacity(ImGuiStyle& style, const ImVec4* base, float opacity);
}
