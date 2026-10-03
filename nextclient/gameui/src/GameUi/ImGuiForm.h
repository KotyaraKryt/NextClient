#pragma once

#include "ImGuiPanel.h"

#include <imgui/imgui.h>

#include <string>

class CScriptObject;

namespace ImGuiForm
{
    constexpr float kCardPadding = 14.0f;
    constexpr float kRounding = 6.0f;

    ImVec4 WithAlpha(ImGuiCol color, float alpha);

    // "1" and "1.000000" are the same value; the engine stores whatever text it was given
    bool SameValue(const char* a, const char* b);

    // a window over the whole screen that dims it and takes the clicks meant for what's behind,
    // with a soft shadow where the dialog was last frame (windowMax.x <= windowMin.x for none yet)
    void DimmedBackdrop(const ImVec2& windowMin, const ImVec2& windowMax);
}

// The settings layout the ImGui dialogs share: cards with rows of caption on the left and control on the right
class CImGuiFormPanel : public CImGuiPanel
{
    DECLARE_CLASS_SIMPLE(CImGuiFormPanel, CImGuiPanel);

public:
    explicit CImGuiFormPanel(const char* layoutFile = nullptr) : BaseClass(layoutFile) {}

protected:
    // a box the settings rows go in, titled unless token is nullptr; it has to be closed
    // before the page's next one
    void BeginCard(const char* token, const char* english);
    void EndCard();
    // a line of smaller text under the next row's caption, like ImGui's SetNext* functions
    void SetNextRowHint(const char* token);
    void SetNextRowHintText(const std::string& text);
    // a row's caption on the left, then the control comes in the right part of the card;
    // pending puts a dot by a row that Apply would change
    void BeginRow(const char* token, bool pending);
    void BeginRowText(const std::string& caption, bool pending);
    void EndRow();

    // a row for one of the options a .scr file describes, editing value; true when it changed
    bool ScriptOptionRow(CScriptObject& option, std::string& value, bool pending);

    // a full-width entry of a list like the options' page list, true when clicked
    static bool ListItem(const char* id, const std::string& text, bool selected);

    // the open card's top left corner and the right edge its controls end at, in screen space
    float m_flCardLeft = 0.0f;
    float m_flCardTop = 0.0f;
    float m_flCardRight = 0.0f;

    // where the open row's caption went, and the hint to put under it
    float m_flRowLeft = 0.0f;
    float m_flRowTop = 0.0f;
    float m_flRowCaptionRight = 0.0f;
    float m_flRowCaptionBottom = 0.0f;
    std::string m_RowHint;
};
