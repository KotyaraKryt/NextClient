#include "ImGuiAppearance.h"
#include "GameUi.h"

#include <cvardef.h>

#include <algorithm>
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
            { "#GameUI_AppearanceConsole", "Console", "con_opacity", "con_fontsize", nullptr },
            { "#GameUI_AppearanceBrowser", "Server browser", "ui_browser_alpha", "ui_browser_font", nullptr },
            { "#GameUI_AppearanceOptions", "Options", "ui_options_alpha", "ui_options_font", nullptr },
            { "#GameUI_AppearanceCreateServer", "New game", "ui_newgame_alpha", "ui_newgame_font", nullptr },
            { "#GameUI_AppearancePlayerList", "Player list", "ui_plist_alpha", "ui_plist_font", nullptr },
            { "#GameUI_AppearanceDialogs", "Questions and messages", "ui_dialogs_alpha", "ui_dialogs_font", "ui_dialogs_dim" },
            { "#GameUI_AppearanceLoading", "Loading screen", nullptr, "ui_loading_font", nullptr },
            { "#GameUI_AppearanceDemoPlayer", "Demo player", "ui_demoui_alpha", "ui_demoui_font", nullptr },
            { "#GameUI_AppearanceScoreboard", "Scoreboard", "ui_scoreboard_alpha", "ui_scoreboard_font", nullptr },
            { "#GameUI_AppearanceMotd", "MOTD", "ui_motd_alpha", "ui_motd_font", "ui_motd_dim" },
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
        }
        return "0";
    }

    void RegisterCvars()
    {
        for (const ElementInfo& info : kElements)
        {
            // GameConsole registers the console's own with its other settings
            if (&info == &Info(Element::Console))
                continue;

            for (const char* cvar : { info.opacityCvar, info.fontCvar, info.dimCvar })
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
}
