#pragma once

#include <imgui/imgui.h>

// The few colours the whole theme is made from; the hovered, pressed and dimmed ones follow from them
struct ThemePalette
{
    enum Color
    {
        Window,
        Field,
        Raised,
        Accent,
        Text,
        Border,
        Count,
    };

    ImVec4 colors[Count];
};

// CS 1.6's olive, sampled from the stock console and server browser
const ThemePalette& DefaultPalette();

// rounded, roomier shapes, and colors made from the palette
void ApplyNextClientTheme(ImGuiStyle& style, const ThemePalette& palette = DefaultPalette());
// the colours alone, for a palette changed while the window is up
void ApplyThemeColors(ImVec4* colors, const ThemePalette& palette);
