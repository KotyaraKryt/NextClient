#include "ImGuiForm.h"
#include "ScriptObject.h"

#include <tier1/strtools.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace ImGuiForm;

namespace
{
    bool ParseNumber(const char* text, float& value)
    {
        char* end;
        value = strtof(text, &end);
        return end != text && *end == '\0';
    }
}

ImVec4 ImGuiForm::WithAlpha(ImGuiCol color, float alpha)
{
    ImVec4 value = ImGui::GetStyleColorVec4(color);
    value.w *= alpha;
    return value;
}

void ImGuiForm::DimmedBackdrop(const ImVec2& windowMin, const ImVec2& windowMax)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##Backdrop", nullptr, flags);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), IM_COL32(0, 0, 0, 150));

    // ImGui has no shadows: a soft one from rings that grow and fade
    if (windowMax.x > windowMin.x)
    {
        constexpr int kLayers = 14;
        for (int i = kLayers; i >= 1; i--)
        {
            float spread = static_cast<float>(i) * 2.0f;
            int alpha = static_cast<int>(10.0f * (1.0f - static_cast<float>(i - 1) / kLayers));
            drawList->AddRectFilled(ImVec2(windowMin.x - spread, windowMin.y - spread + 6.0f), ImVec2(windowMax.x + spread, windowMax.y + spread + 6.0f),
                IM_COL32(0, 0, 0, alpha), kRounding + spread);
        }
    }
    ImGui::End();
}

bool ImGuiForm::SameValue(const char* a, const char* b)
{
    if (!strcmp(a, b))
        return true;

    float x, y;
    return ParseNumber(a, x) && ParseNumber(b, y) && std::fabs(x - y) < 0.0001f;
}

void CImGuiFormPanel::BeginCard(const char* token, const char* english)
{
    // the content goes on the top channel, so the background can be drawn under it once its height is known
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->ChannelsSplit(2);
    drawList->ChannelsSetCurrent(1);

    ImVec2 start = ImGui::GetCursorScreenPos();
    m_flCardLeft = start.x;
    m_flCardTop = start.y;
    m_flCardRight = start.x + ImGui::GetContentRegionAvail().x - kCardPadding;

    ImGui::SetCursorScreenPos(ImVec2(start.x + kCardPadding, start.y + kCardPadding));
    ImGui::BeginGroup();
    if (token)
    {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", Localized(token, english).c_str());
        ImGui::Dummy(ImVec2(0, 2));
    }
}

void CImGuiFormPanel::EndCard()
{
    ImGui::EndGroup();

    ImVec2 min(m_flCardLeft, m_flCardTop);
    ImVec2 max(m_flCardRight + kCardPadding, ImGui::GetItemRectMax().y + kCardPadding);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->ChannelsSetCurrent(0);
    drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_WindowBg), kRounding);
    drawList->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border), kRounding);
    drawList->ChannelsMerge();

    // a dummy as wide as the card, so the page knows how much room it took
    ImGui::SetCursorScreenPos(min);
    ImGui::Dummy(ImVec2(max.x - min.x, max.y - min.y));
    ImGui::Dummy(ImVec2(0, 4));
}

void CImGuiFormPanel::SetNextRowHint(const char* token)
{
    m_RowHint = Localized(token);
}

void CImGuiFormPanel::SetNextRowHintText(const std::string& text)
{
    m_RowHint = text;
}

void CImGuiFormPanel::BeginRow(const char* token, bool pending)
{
    BeginRowText(Localized(token), pending);
}

void CImGuiFormPanel::BeginRowText(const std::string& caption, bool pending)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float innerWidth = m_flCardRight - pos.x;
    float controlWidth = std::clamp(innerWidth * 0.5f, 180.0f, 340.0f);
    float controlX = m_flCardRight - controlWidth;

    m_flRowLeft = pos.x;
    m_flRowTop = pos.y;
    m_flRowCaptionRight = controlX - ImGui::GetStyle().ItemSpacing.x;

    if (pending)
    {
        ImVec2 dot(pos.x - kCardPadding * 0.5f, pos.y + ImGui::GetFrameHeight() * 0.5f);
        ImGui::GetWindowDrawList()->AddCircleFilled(dot, 3.0f, ImGui::GetColorU32(ImGuiCol_CheckMark));
    }

    // a caption longer than its room wraps, and the row grows to fit it
    float captionRoom = m_flRowCaptionRight - pos.x;
    ImGui::AlignTextToFramePadding();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + captionRoom);
    ImGui::TextUnformatted(caption.c_str());
    ImGui::PopTextWrapPos();
    m_flRowCaptionBottom = ImGui::GetItemRectMax().y;

    ImGui::SameLine();
    ImGui::SetCursorScreenPos(ImVec2(controlX, pos.y));
    ImGui::SetNextItemWidth(controlWidth);
}

void CImGuiFormPanel::EndRow()
{
    float controlBottom = ImGui::GetItemRectMax().y;
    float textBottom = m_flRowCaptionBottom;

    std::string hint = std::move(m_RowHint);
    m_RowHint.clear();
    if (!hint.empty())
    {
        // under the caption, where the control has left the cursor below itself
        ImGui::SetCursorScreenPos(ImVec2(m_flRowLeft, m_flRowCaptionBottom));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (m_flRowCaptionRight - m_flRowLeft));
        ImGui::TextDisabled("%s", hint.c_str());
        ImGui::PopTextWrapPos();
        textBottom = ImGui::GetItemRectMax().y + 2.0f;
    }

    // the next row starts below whichever is taller, the control or the text beside it; an empty
    // item there tells the card where the row ends and moves the cursor on by the item spacing
    ImGui::SetCursorScreenPos(ImVec2(m_flRowLeft, std::max(controlBottom, textBottom)));
    ImGui::Dummy(ImVec2(0, 0));
}

bool CImGuiFormPanel::ScriptOptionRow(CScriptObject& option, std::string& value, bool pending)
{
    // the prompts are mostly tokens, but a script may have plain text too
    std::string caption = option.prompt[0] == '#' ? Localized(option.prompt, option.prompt + 1) : option.prompt;
    bool changed = false;

    ImGui::PushID(option.cvarname);
    switch (option.type)
    {
        case O_BOOL:
        {
            BeginRowText(caption, pending);
            bool checked = atof(value.c_str()) != 0.0;
            if (ImGui::Checkbox("##Value", &checked))
            {
                value = checked ? "1" : "0";
                changed = true;
            }
            EndRow();
            break;
        }

        case O_NUMBER:
        {
            BeginRowText(caption, pending);
            float number = static_cast<float>(atof(value.c_str()));
            // -1 for both ends means no limits, which a slider can't show
            if (option.fMin == -1.0f && option.fMax == -1.0f)
                changed = ImGui::InputFloat("##Value", &number, 0.0f, 0.0f, "%g");
            else if (option.fMax == -1.0f)
            {
                changed = ImGui::InputFloat("##Value", &number, 0.0f, 0.0f, "%g");
                number = std::max(number, option.fMin);
            }
            else
                changed = ImGui::SliderFloat("##Value", &number, option.fMin, option.fMax, "%g", ImGuiSliderFlags_AlwaysClamp);
            if (changed)
            {
                char text[32];
                snprintf(text, sizeof(text), "%g", number);
                value = text;
            }
            EndRow();
            break;
        }

        case O_STRING:
        {
            BeginRowText(caption, pending);
            char text[128];
            V_strncpy(text, value.c_str(), sizeof(text));
            if (ImGui::InputText("##Value", text, sizeof(text)))
            {
                // quotes would end the command that sets the value early
                UTIL_StripInvalidCharacters(text, sizeof(text));
                value = text;
                changed = true;
            }
            EndRow();
            break;
        }

        case O_LIST:
        {
            BeginRowText(caption, pending);
            auto itemText = [](const CScriptListItem* item)
            {
                return item->szItemText[0] == '#' ? Localized(item->szItemText, item->szItemText + 1) : std::string(item->szItemText);
            };

            std::string preview = value;
            for (CScriptListItem* item = option.pListItems; item; item = item->pNext)
            {
                if (SameValue(item->szValue, value.c_str()))
                    preview = itemText(item);
            }

            if (ImGui::BeginCombo("##Value", preview.c_str()))
            {
                for (CScriptListItem* item = option.pListItems; item; item = item->pNext)
                {
                    if (ImGui::Selectable(itemText(item).c_str(), SameValue(item->szValue, value.c_str())))
                    {
                        value = item->szValue;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            EndRow();
            break;
        }

        default:
            break;
    }
    ImGui::PopID();

    return changed;
}

bool CImGuiFormPanel::ListItem(const char* id, const std::string& text, bool selected)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float width = ImGui::GetContentRegionAvail().x;
    float height = ImGui::GetFrameHeight() + 8.0f;
    float textOffset = (height - ImGui::GetTextLineHeight()) * 0.5f;

    // the accent is too dark to read on its own tint, so the selected item's text gets a lighter one
    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    ImVec4 bright(accent.x + (1.0f - accent.x) * 0.45f, accent.y + (1.0f - accent.y) * 0.45f, accent.z + (1.0f - accent.z) * 0.45f, 1.0f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 end(pos.x + width, pos.y + height);

    bool clicked = ImGui::InvisibleButton(id, ImVec2(width, height));

    if (selected)
    {
        drawList->AddRectFilled(pos, end, ImGui::GetColorU32(WithAlpha(ImGuiCol_CheckMark, 0.2f)), kRounding, ImDrawFlags_RoundCornersRight);
        drawList->AddRectFilled(pos, ImVec2(pos.x + 3.0f, end.y), ImGui::GetColorU32(bright));
    }
    else if (ImGui::IsItemHovered())
        drawList->AddRectFilled(pos, end, ImGui::GetColorU32(WithAlpha(ImGuiCol_Text, 0.06f)), kRounding);

    ImU32 textColor = ImGui::GetColorU32(selected ? bright : ImGui::GetStyleColorVec4(ImGuiCol_Text));
    drawList->PushClipRect(pos, ImVec2(end.x - 6.0f, end.y), true);
    drawList->AddText(ImVec2(pos.x + 14.0f, pos.y + textOffset), textColor, text.c_str());
    drawList->PopClipRect();

    return clicked;
}
