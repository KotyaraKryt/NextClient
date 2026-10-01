#include "ImGuiTheme.h"

#include <imgui/imgui.h>

static ImVec4 Rgb(int r, int g, int b, float alpha = 1.0f)
{
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, alpha);
}

void ApplyNextClientTheme(ImGuiStyle& style)
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

    // sampled from the stock CS 1.6 console and server browser
    const ImVec4 window = Rgb(76, 88, 68);    // frame background
    const ImVec4 field = Rgb(62, 70, 55);     // text area and input line
    const ImVec4 raised = Rgb(90, 106, 80);   // scrollbar
    const ImVec4 accent = Rgb(142, 137, 35);  // selected server row
    const ImVec4 text = Rgb(216, 222, 211);
    const ImVec4 border = Rgb(41, 46, 36);

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = Rgb(150, 160, 140);
    c[ImGuiCol_WindowBg] = window;
    c[ImGuiCol_ChildBg] = field;
    c[ImGuiCol_PopupBg] = field;
    c[ImGuiCol_Border] = border;
    c[ImGuiCol_BorderShadow] = Rgb(0, 0, 0, 0.0f);

    c[ImGuiCol_FrameBg] = field;
    c[ImGuiCol_FrameBgHovered] = Rgb(70, 80, 62);
    c[ImGuiCol_FrameBgActive] = Rgb(70, 80, 62);

    c[ImGuiCol_TitleBg] = window;
    c[ImGuiCol_TitleBgActive] = window;
    c[ImGuiCol_TitleBgCollapsed] = window;
    c[ImGuiCol_MenuBarBg] = window;

    c[ImGuiCol_ScrollbarBg] = Rgb(0, 0, 0, 0.0f);
    c[ImGuiCol_ScrollbarGrab] = raised;
    c[ImGuiCol_ScrollbarGrabHovered] = Rgb(110, 128, 98);
    c[ImGuiCol_ScrollbarGrabActive] = accent;

    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = raised;
    c[ImGuiCol_SliderGrabActive] = accent;

    c[ImGuiCol_Button] = raised;
    c[ImGuiCol_ButtonHovered] = Rgb(110, 128, 98);
    c[ImGuiCol_ButtonActive] = accent;

    c[ImGuiCol_Header] = raised;
    c[ImGuiCol_HeaderHovered] = Rgb(110, 128, 98);
    c[ImGuiCol_HeaderActive] = accent;

    c[ImGuiCol_Separator] = border;
    c[ImGuiCol_SeparatorHovered] = accent;
    c[ImGuiCol_SeparatorActive] = accent;

    c[ImGuiCol_ResizeGrip] = Rgb(0, 0, 0, 0.0f);
    c[ImGuiCol_ResizeGripHovered] = accent;
    c[ImGuiCol_ResizeGripActive] = accent;

    c[ImGuiCol_Tab] = field;
    c[ImGuiCol_TabHovered] = raised;
    c[ImGuiCol_TabActive] = raised;
    c[ImGuiCol_TabUnfocused] = field;
    c[ImGuiCol_TabUnfocusedActive] = raised;

    c[ImGuiCol_TextSelectedBg] = Rgb(142, 137, 35, 0.6f);
    c[ImGuiCol_NavHighlight] = accent;
}
