#include "ImGuiTheme.h"

static ImVec4 Rgb(int r, int g, int b, float alpha = 1.0f)
{
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, alpha);
}

// toward white by amount, or toward black for a negative one
static ImVec4 Shade(const ImVec4& color, float amount)
{
    float target = amount > 0.0f ? 1.0f : 0.0f;
    float t = amount > 0.0f ? amount : -amount;
    return ImVec4(color.x + (target - color.x) * t, color.y + (target - color.y) * t, color.z + (target - color.z) * t, color.w);
}

static ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

static ImVec4 WithAlpha(ImVec4 color, float alpha)
{
    color.w = alpha;
    return color;
}

const ThemePalette& DefaultPalette()
{
    static const ThemePalette palette = { {
        Rgb(76, 88, 68),    // frame background
        Rgb(62, 70, 55),    // text area and input line
        Rgb(90, 106, 80),   // scrollbar
        Rgb(142, 137, 35),  // selected server row
        Rgb(216, 222, 211),
        Rgb(41, 46, 36),
    } };
    return palette;
}

void ApplyNextClientTheme(ImGuiStyle& style, const ThemePalette& palette)
{
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(8, 5);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.ScrollbarSize = 10;

    style.WindowRounding = 6;
    style.ChildRounding = 4;
    style.FrameRounding = 4;
    style.PopupRounding = 4;
    style.ScrollbarRounding = 4;
    style.GrabRounding = 4;
    style.TabRounding = 4;

    style.WindowBorderSize = 1;
    style.ChildBorderSize = 1;
    style.FrameBorderSize = 0;
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

    ApplyThemeColors(style.Colors, palette);
}

void ApplyThemeColors(ImVec4* c, const ThemePalette& palette)
{
    const ImVec4& window = palette.colors[ThemePalette::Window];
    const ImVec4& field = palette.colors[ThemePalette::Field];
    const ImVec4& raised = palette.colors[ThemePalette::Raised];
    const ImVec4& accent = palette.colors[ThemePalette::Accent];
    const ImVec4& text = palette.colors[ThemePalette::Text];
    const ImVec4& border = palette.colors[ThemePalette::Border];

    // the olive theme's hovered colours were these steps lighter than the ones under them
    const ImVec4 raisedHovered = Shade(raised, 0.14f);
    const ImVec4 fieldHovered = Shade(field, 0.07f);
    const ImVec4 clear = Rgb(0, 0, 0, 0.0f);

    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = Mix(text, window, 0.4f);
    c[ImGuiCol_WindowBg] = window;
    c[ImGuiCol_ChildBg] = field;
    c[ImGuiCol_PopupBg] = field;
    c[ImGuiCol_Border] = border;
    c[ImGuiCol_BorderShadow] = clear;

    c[ImGuiCol_FrameBg] = field;
    c[ImGuiCol_FrameBgHovered] = fieldHovered;
    c[ImGuiCol_FrameBgActive] = fieldHovered;

    c[ImGuiCol_TitleBg] = window;
    c[ImGuiCol_TitleBgActive] = window;
    c[ImGuiCol_TitleBgCollapsed] = window;
    c[ImGuiCol_MenuBarBg] = window;

    c[ImGuiCol_ScrollbarBg] = clear;
    c[ImGuiCol_ScrollbarGrab] = raised;
    c[ImGuiCol_ScrollbarGrabHovered] = raisedHovered;
    c[ImGuiCol_ScrollbarGrabActive] = accent;

    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = raised;
    c[ImGuiCol_SliderGrabActive] = accent;

    c[ImGuiCol_Button] = raised;
    c[ImGuiCol_ButtonHovered] = raisedHovered;
    c[ImGuiCol_ButtonActive] = accent;

    c[ImGuiCol_Header] = raised;
    c[ImGuiCol_HeaderHovered] = raisedHovered;
    c[ImGuiCol_HeaderActive] = accent;

    c[ImGuiCol_Separator] = border;
    c[ImGuiCol_SeparatorHovered] = accent;
    c[ImGuiCol_SeparatorActive] = accent;

    c[ImGuiCol_ResizeGrip] = clear;
    c[ImGuiCol_ResizeGripHovered] = accent;
    c[ImGuiCol_ResizeGripActive] = accent;

    c[ImGuiCol_Tab] = field;
    c[ImGuiCol_TabHovered] = raised;
    c[ImGuiCol_TabActive] = raised;
    c[ImGuiCol_TabUnfocused] = field;
    c[ImGuiCol_TabUnfocusedActive] = raised;

    // the server list's column headers are flat like the window, as in the stock browser
    c[ImGuiCol_TableHeaderBg] = window;
    c[ImGuiCol_TableBorderStrong] = border;
    c[ImGuiCol_TableBorderLight] = border;
    c[ImGuiCol_TableRowBg] = clear;
    c[ImGuiCol_TableRowBgAlt] = Rgb(255, 255, 255, 0.03f);
    c[ImGuiCol_PlotHistogram] = accent;

    c[ImGuiCol_TextSelectedBg] = WithAlpha(accent, 0.6f);
    c[ImGuiCol_NavHighlight] = accent;
}
