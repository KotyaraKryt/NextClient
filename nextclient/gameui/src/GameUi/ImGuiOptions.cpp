#include "ImGuiOptions.h"
#include "BasePanel.h"
#include "GameUi.h"
#include "ModInfo.h"

#include <cvardef.h>
#include <tier1/strtools.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

namespace
{
    cvar_t* g_pLegacyCvar = nullptr;

    constexpr float kSidebarWidth = 190.0f;
    constexpr float kPagePadding = 18.0f;
    constexpr float kCardPadding = 14.0f;
    constexpr float kRounding = 6.0f;

    enum
    {
        kGroupPlayer,
        kGroupControls,
        kGroupSystem,
    };

    struct Group
    {
        const char* token;
        const char* english;
    };

    const Group kGroups[] = {
        { "#GameUI_OptionsGroupPlayer", "Player" },
        { "#GameUI_OptionsGroupControls", "Controls" },
        { "#GameUI_OptionsGroupSystem", "System" },
    };

    ImVec4 WithAlpha(ImGuiCol color, float alpha)
    {
        ImVec4 value = ImGui::GetStyleColorVec4(color);
        value.w *= alpha;
        return value;
    }

    bool ParseNumber(const char* text, float& value)
    {
        char* end;
        value = strtof(text, &end);
        return end != text && *end == '\0';
    }

    // "1" and "1.000000" are the same value; the engine stores whatever text it was given
    bool SameValue(const char* a, const char* b)
    {
        if (!strcmp(a, b))
            return true;

        float x, y;
        return ParseNumber(a, x) && ParseNumber(b, y) && std::fabs(x - y) < 0.0001f;
    }
}

CImGuiOptions::CImGuiOptions() : BaseClass("options_layout.ini")
{
    SetVisible(false);

    // the old dialog's pages, sorted into the sidebar's groups
    bool singlePlayerOnly = ModInfo().IsSinglePlayerOnly();
    if (!singlePlayerOnly)
        m_Pages.push_back({ "multiplayer", "#GameUI_Multiplayer", kGroupPlayer });
    m_Pages.push_back({ "game", "#GameUI_Game", kGroupPlayer });
    m_Pages.push_back({ "keyboard", "#GameUI_Keyboard", kGroupControls });
    m_Pages.push_back({ "mouse", "#GameUI_Mouse", kGroupControls });
    m_Pages.push_back({ "audio", "#GameUI_Audio", kGroupSystem, &CImGuiOptions::DrawAudio, "#GameUI_OptionsAudioHint", "Volume and sound quality" });
    m_Pages.push_back({ "video", "#GameUI_Video", kGroupSystem });
    if (!singlePlayerOnly)
        m_Pages.push_back({ "voice", "#GameUI_Voice", kGroupSystem });
    m_Pages.push_back({ "miscellaneous", "#GameUI_Miscellaneous", kGroupSystem });

    for (const Page& page : m_Pages)
    {
        if (page.draw && !m_pSelected)
            m_pSelected = &page;
    }
}

void CImGuiOptions::RegisterCvars()
{
    g_pLegacyCvar = engine->pfnRegisterVariable("opt_legacy", "0", FCVAR_ARCHIVE);
}

bool CImGuiOptions::UseLegacyDialog()
{
    return g_pLegacyCvar && g_pLegacyCvar->value != 0.0f;
}

void CImGuiOptions::Activate(const char* tabName)
{
    // like the old dialog, every opening starts from what the cvars are now
    if (!IsVisible())
        m_Pending.clear();

    if (tabName)
    {
        for (const Page& page : m_Pages)
        {
            if (!V_stricmp(page.id, tabName))
                m_pSelected = &page;
        }
    }

    SetVisible(true);
    MoveToFront();
    RequestFocus();
    ResetInput();
    m_bFocusWindow = true;
}

void CImGuiOptions::Close()
{
    m_Pending.clear();
    SetVisible(false);
}

void CImGuiOptions::DrawImGui()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(880, 580), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(720, 440), ImVec2(FLT_MAX, FLT_MAX));

    // without the border a checkbox is a blank square
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    if (m_bFocusWindow)
    {
        ImGui::SetNextWindowFocus();
        m_bFocusWindow = false;
    }

    bool open = true;
    std::string title = Localized("#GameUI_Options") + "###Options";
    if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse))
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        float footer = ImGui::GetFrameHeight() + style.ItemSpacing.y * 2.0f + style.WindowPadding.y;

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kRounding);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 12));
        ImGui::BeginChild("Pages", ImVec2(kSidebarWidth, -footer), true);
        DrawPageList();
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPagePadding, kPagePadding));
        ImGui::BeginChild("Page", ImVec2(0, -footer), true);
        if (m_pSelected)
        {
            DrawPageHeading(*m_pSelected);
            if (m_pSelected->draw)
                (this->*m_pSelected->draw)();
            else
                DrawNotPorted(*m_pSelected);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);

        DrawFooter();

        // in a level, Escape belongs to the game menu
        bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
        if (focused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape) && !GameUI().IsInLevel())
            open = false;
    }
    ImGui::End();

    ImGui::PopStyleVar();

    if (!open)
        Close();
}

void CImGuiOptions::DrawPageList()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float width = ImGui::GetContentRegionAvail().x;
    float height = ImGui::GetFrameHeight() + 8.0f;
    float textOffset = (height - ImGui::GetTextLineHeight()) * 0.5f;

    // the accent is too dark to read on its own tint, so the selected page's text gets a lighter one
    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    ImVec4 bright(accent.x + (1.0f - accent.x) * 0.45f, accent.y + (1.0f - accent.y) * 0.45f, accent.z + (1.0f - accent.z) * 0.45f, 1.0f);

    // the pages still to come get an arrow: they open in the old dialog
    const char* legacyMark = "↗";
    float legacyMarkWidth = ImGui::CalcTextSize(legacyMark).x;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));

    int group = -1;
    for (const Page& page : m_Pages)
    {
        if (page.group != group)
        {
            if (group != -1)
                ImGui::Dummy(ImVec2(0, 10));

            group = page.group;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.0f);
            ImGui::TextColored(accent, "%s", Localized(kGroups[group].token, kGroups[group].english).c_str());
            ImGui::Dummy(ImVec2(0, 2));
        }

        bool selected = m_pSelected == &page;
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 end(pos.x + width, pos.y + height);

        if (ImGui::InvisibleButton(page.id, ImVec2(width, height)))
            m_pSelected = &page;
        bool hovered = ImGui::IsItemHovered();

        if (selected)
        {
            drawList->AddRectFilled(pos, end, ImGui::GetColorU32(WithAlpha(ImGuiCol_CheckMark, 0.2f)), kRounding, ImDrawFlags_RoundCornersRight);
            drawList->AddRectFilled(pos, ImVec2(pos.x + 3.0f, end.y), ImGui::GetColorU32(bright));
        }
        else if (hovered)
            drawList->AddRectFilled(pos, end, ImGui::GetColorU32(WithAlpha(ImGuiCol_Text, 0.06f)), kRounding);

        ImU32 textColor = ImGui::GetColorU32(selected ? bright : ImGui::GetStyleColorVec4(ImGuiCol_Text));
        drawList->AddText(ImVec2(pos.x + 14.0f, pos.y + textOffset), textColor, Localized(page.token).c_str());

        if (!page.draw)
        {
            ImVec2 markPos(end.x - legacyMarkWidth - 10.0f, pos.y + textOffset);
            drawList->AddText(markPos, ImGui::GetColorU32(ImGuiCol_TextDisabled), legacyMark);
            if (hovered)
                ImGui::SetTooltip("%s", Localized("#GameUI_OptionsNotPorted", "This page isn't in the new options yet.").c_str());
        }
    }

    ImGui::PopStyleVar();
}

void CImGuiOptions::DrawPageHeading(const Page& page)
{
    ImGui::PushFont(HeadingFont());
    ImGui::TextUnformatted(Localized(page.token).c_str());
    ImGui::PopFont();

    if (page.hintToken)
        ImGui::TextDisabled("%s", Localized(page.hintToken, page.hintEnglish).c_str());

    ImGui::Dummy(ImVec2(0, 8));
}

void CImGuiOptions::DrawFooter()
{
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::Dummy(ImVec2(0, style.ItemSpacing.y));

    if (!m_Pending.empty())
    {
        // the localized text says where the number goes with a %d
        std::string text = Localized("#GameUI_OptionsUnapplied", "Unapplied changes: %d");
        size_t at = text.find("%d");
        if (at != std::string::npos)
            text.replace(at, 2, std::to_string(m_Pending.size()));

        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", text.c_str());
        ImGui::SameLine();
    }

    std::string ok = Localized("#GameUI_OK");
    std::string cancel = Localized("#GameUI_Cancel");
    std::string apply = Localized("#GameUI_Apply");

    float buttonWidth = 96.0f;
    for (const std::string* text : { &ok, &cancel, &apply })
        buttonWidth = std::max(buttonWidth, ImGui::CalcTextSize(text->c_str()).x + style.FramePadding.x * 2.0f);

    float rowWidth = buttonWidth * 3.0f + style.ItemSpacing.x * 2.0f;
    float rowStart = ImGui::GetWindowContentRegionMax().x - rowWidth;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), rowStart));

    if (ImGui::Button(ok.c_str(), ImVec2(buttonWidth, 0)))
    {
        ApplyChanges();
        Close();
    }

    ImGui::SameLine();
    if (ImGui::Button(cancel.c_str(), ImVec2(buttonWidth, 0)))
        Close();

    // Apply stands out while there is something for it to do
    ImGui::SameLine();
    bool pending = !m_Pending.empty();
    if (pending)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(ImGuiCol_CheckMark, 0.85f));
    }
    ImGui::BeginDisabled(!pending);
    if (ImGui::Button(apply.c_str(), ImVec2(buttonWidth, 0)))
        ApplyChanges();
    ImGui::EndDisabled();
    if (pending)
        ImGui::PopStyleColor(2);
}

void CImGuiOptions::DrawNotPorted(const Page& page)
{
    std::string text = Localized("#GameUI_OptionsNotPorted", "This page isn't in the new options yet.");
    std::string button = Localized("#GameUI_OptionsOpenLegacy", "Open it in the old options");

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float left = ImGui::GetCursorPosX();
    float textWidth = ImGui::CalcTextSize(text.c_str()).x;
    float buttonWidth = ImGui::CalcTextSize(button.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    float blockHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetFrameHeight();

    ImGui::SetCursorPos(ImVec2(left + (avail.x - textWidth) * 0.5f, ImGui::GetCursorPosY() + (avail.y - blockHeight) * 0.5f));
    ImGui::TextDisabled("%s", text.c_str());

    ImGui::SetCursorPosX(left + (avail.x - buttonWidth) * 0.5f);
    if (ImGui::Button(button.c_str()))
    {
        Close();
        BasePanel()->OpenLegacyOptionsDialog(page.id);
    }
}

void CImGuiOptions::DrawAudio()
{
    BeginCard("#GameUI_OptionsVolume", "Volume");
    CvarSlider("#GameUI_SoundEffectVolume", "volume", 0.0f, 2.0f, "%.0f%%", 100.0f);

    // the suit only talks in Half-Life
    if (!ModInfo().IsMultiplayerOnly())
        CvarSlider("#GameUI_HEVSuitVolume", "suitvolume", 0.0f, 2.0f, "%.0f%%", 100.0f);

    CvarSlider("#GameUI_MP3Volume", "mp3volume", 0.0f, 1.0f, "%.0f%%", 100.0f);
    EndCard();

    BeginCard("#GameUI_OptionsSound", "Sound");
    CvarCombo("#GameUI_SoundQuality", "hisound", { { "#GameUI_High", "1" }, { "#GameUI_Low", "0" } });

    // only engines old enough to have them still register these
    CvarCheckbox("#GameUI_EnableEAX", "s_eax");
    CvarCheckbox("#GameUI_EnableA3D", "s_a3d");
    EndCard();

    // the MP3 volume's caption ends with a * that points at this
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", Localized("#GameUI_Miles_Audio").c_str());
    ImGui::PopTextWrapPos();
}

void CImGuiOptions::BeginCard(const char* token, const char* english)
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
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", Localized(token, english).c_str());
    ImGui::Dummy(ImVec2(0, 2));
}

void CImGuiOptions::EndCard()
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

void CImGuiOptions::SettingRow(const char* token, const char* cvar)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float innerWidth = m_flCardRight - pos.x;
    float controlWidth = std::clamp(innerWidth * 0.5f, 180.0f, 340.0f);
    float controlX = m_flCardRight - controlWidth;

    // a dot in the card's padding marks a row that Apply would change
    if (m_Pending.count(cvar))
    {
        ImVec2 dot(pos.x - kCardPadding * 0.5f, pos.y + ImGui::GetFrameHeight() * 0.5f);
        ImGui::GetWindowDrawList()->AddCircleFilled(dot, 3.0f, ImGui::GetColorU32(ImGuiCol_CheckMark));
    }

    // a caption longer than its room is cut off and shown whole on hover
    std::string caption = Localized(token);
    float captionRoom = controlX - pos.x - ImGui::GetStyle().ItemSpacing.x;
    ImGui::AlignTextToFramePadding();
    ImGui::PushClipRect(pos, ImVec2(pos.x + captionRoom, pos.y + ImGui::GetFrameHeight()), true);
    ImGui::TextUnformatted(caption.c_str());
    ImGui::PopClipRect();
    if (ImGui::CalcTextSize(caption.c_str()).x > captionRoom && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", caption.c_str());

    ImGui::SameLine();
    ImGui::SetCursorScreenPos(ImVec2(controlX, pos.y));
    ImGui::SetNextItemWidth(controlWidth);
}

bool CImGuiOptions::CvarCheckbox(const char* token, const char* cvar)
{
    if (!engine->pfnGetCvarPointer(cvar))
        return false;

    SettingRow(token, cvar);

    bool value = PendingValue(cvar) != 0.0f;
    ImGui::PushID(cvar);
    bool changed = ImGui::Checkbox("##Value", &value);
    ImGui::PopID();

    if (changed)
        SetPending(cvar, value ? "1" : "0");
    return changed;
}

bool CImGuiOptions::CvarSlider(const char* token, const char* cvar, float min, float max, const char* format, float displayScale)
{
    if (!engine->pfnGetCvarPointer(cvar))
        return false;

    SettingRow(token, cvar);

    float shown = PendingValue(cvar) * displayScale;
    ImGui::PushID(cvar);
    bool changed = ImGui::SliderFloat("##Value", &shown, min * displayScale, max * displayScale, format, ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopID();

    if (changed)
    {
        char value[32];
        snprintf(value, sizeof(value), "%g", shown / displayScale);
        SetPending(cvar, value);
    }
    return changed;
}

bool CImGuiOptions::CvarCombo(const char* token, const char* cvar, const std::vector<Choice>& choices)
{
    if (!engine->pfnGetCvarPointer(cvar))
        return false;

    SettingRow(token, cvar);

    // a value none of the choices give, set from the console, shows as it is
    std::string current = PendingString(cvar);
    std::string preview = current;
    for (const Choice& choice : choices)
    {
        if (SameValue(choice.value, current.c_str()))
            preview = Localized(choice.token);
    }

    bool changed = false;
    ImGui::PushID(cvar);
    if (ImGui::BeginCombo("##Value", preview.c_str()))
    {
        for (const Choice& choice : choices)
        {
            bool selected = SameValue(choice.value, current.c_str());
            if (ImGui::Selectable(Localized(choice.token).c_str(), selected) && !selected)
            {
                SetPending(cvar, choice.value);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();

    return changed;
}

std::string CImGuiOptions::PendingString(const char* cvar) const
{
    auto it = m_Pending.find(cvar);
    if (it != m_Pending.end())
        return it->second;

    cvar_t* pointer = engine->pfnGetCvarPointer(cvar);
    return pointer ? pointer->string : "";
}

float CImGuiOptions::PendingValue(const char* cvar) const
{
    return static_cast<float>(atof(PendingString(cvar).c_str()));
}

void CImGuiOptions::SetPending(const char* cvar, const std::string& value)
{
    // a control moved back to where it was leaves nothing to apply
    cvar_t* pointer = engine->pfnGetCvarPointer(cvar);
    if (pointer && SameValue(pointer->string, value.c_str()))
        m_Pending.erase(cvar);
    else
        m_Pending[cvar] = value;
}

void CImGuiOptions::ApplyChanges()
{
    for (const auto& [cvar, value] : m_Pending)
        engine->Cvar_Set(cvar.c_str(), value.c_str());

    m_Pending.clear();
}
