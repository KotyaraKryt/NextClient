#include "ImGuiOptions.h"
#include "BasePanel.h"
#include "GameUi.h"
#include "IGameUIFuncs.h"
#include "ivoicetweak.h"
#include "OptionsDialog/VideoAdvancedDialog.h"

#include <nitro_utils/config/FileConfigProvider.h>
#include "ModInfo.h"
#include "OptionsDialog/OptionsSubMiscellaneous.h"

#include <FileSystem.h>
#include <KeyValues.h>

#include <cvardef.h>
#include <tier1/strtools.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <numeric>
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
    m_Pages.push_back({ "mouse", "#GameUI_Mouse", kGroupControls, &CImGuiOptions::DrawMouse, "#GameUI_OptionsMouseHint", "Sensitivity, looking around and the joystick" });
    m_Pages.push_back({ "audio", "#GameUI_Audio", kGroupSystem, &CImGuiOptions::DrawAudio, "#GameUI_OptionsAudioHint", "Volume and sound quality" });
    m_Pages.push_back({ "video", "#GameUI_Video", kGroupSystem, &CImGuiOptions::DrawVideo, "#GameUI_OptionsVideoHint", "Screen, picture and field of view" });
    if (!singlePlayerOnly)
        m_Pages.push_back({ "voice", "#GameUI_Voice", kGroupSystem, &CImGuiOptions::DrawVoice, "#GameUI_OptionsVoiceHint", "Voice chat and the microphone" });
    m_Pages.push_back({ "miscellaneous", "#GameUI_Miscellaneous", kGroupSystem, &CImGuiOptions::DrawMisc, "#GameUI_OptionsMiscHint", "Look of the menus and the server browser" });

    for (const Page& page : m_Pages)
    {
        if (page.draw && !m_pSelected)
            m_pSelected = &page;
    }
}

CImGuiOptions::~CImGuiOptions()
{
    StopMicrophoneTest();
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
    {
        m_Pending.clear();
        m_PendingKeys.clear();
        LoadMiscSettings();
        LoadVoiceSettings();
        LoadVideoSettings();
    }

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
    StopMicrophoneTest();
    m_VoiceEdited = m_VoiceSaved;
    m_VideoEdited = m_VideoSaved;
    m_Pending.clear();
    m_PendingKeys.clear();
    m_MiscEdited = m_MiscSaved;
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

        // the test keeps the microphone open, so it ends with the page
        if (m_bTestingMicrophone && (!m_pSelected || m_pSelected->draw != &CImGuiOptions::DrawVoice))
            StopMicrophoneTest();

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

    if (PendingCount() > 0)
    {
        // the localized text says where the number goes with a %d
        std::string text = Localized("#GameUI_OptionsUnapplied", "Unapplied changes: %d");
        size_t at = text.find("%d");
        if (at != std::string::npos)
            text.replace(at, 2, std::to_string(PendingCount()));

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
    bool pending = PendingCount() > 0;
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

void CImGuiOptions::DrawMouse()
{
    BeginCard("#GameUI_Mouse", "Mouse");
    // most players sit between 1 and 5, which a logarithmic slider gives the most room to
    CvarSlider("#GameUI_MouseSensitivity", "sensitivity", 0.2f, 20.0f, "%.2f", 1.0f, ImGuiSliderFlags_Logarithmic);

    SetNextRowHint("#GameUI_ReverseMouseLabel");
    CvarNegateCheckbox("#GameUI_ReverseMouse", "m_pitch");
    SetNextRowHint("#GameUI_MouseLookLabel");
    KeyToggleCheckbox("#GameUI_MouseLook", "in_mlook", "mlook");
    SetNextRowHint("#GameUI_MouseFilterLabel");
    CvarCheckbox("#GameUI_MouseFilter", "m_filter");
    SetNextRowHint("#GameUI_RawInputLabel");
    CvarCheckbox("#GameUI_RawInput", "m_rawinput");
    SetNextRowHint("#GameUI_AutoaimLabel");
    CvarCheckbox("#GameUI_AutoAim", "sv_aim");
    EndCard();

    BeginCard("#GameUI_Joystick", "Joystick");
    SetNextRowHint("#GameUI_JoystickLabel");
    CvarCheckbox("#GameUI_Joystick", "joystick");
    SetNextRowHint("#GameUI_JoystickLookLabel");
    KeyToggleCheckbox("#GameUI_JoystickLook", "in_jlook", "jlook");
    EndCard();

    ImGui::TextDisabled("%s", Localized("#GameUI_OptionsSliderTyping", "Ctrl+click a slider to type a value").c_str());
}

void CImGuiOptions::DrawMisc()
{
    BeginCard("#GameUI_OptionsLook", "Look");

    BeginRow("#GameUI_ColorScheme", m_MiscEdited.scheme != m_MiscSaved.scheme);
    std::string current = OptionsSubMiscellaneous::MakeSchemeName(m_MiscEdited.scheme);
    if (ImGui::BeginCombo("##Scheme", current.c_str()))
    {
        for (const std::string& scheme : m_Schemes)
        {
            bool selected = scheme == m_MiscEdited.scheme;
            if (ImGui::Selectable(OptionsSubMiscellaneous::MakeSchemeName(scheme).c_str(), selected))
                m_MiscEdited.scheme = scheme;
        }
        ImGui::EndCombo();
    }
    SetNextRowHint("#GameUI_OptionsSchemeRestart");
    EndRow();
    EndCard();

    BeginCard("#ServerBrowser_Servers", "Servers");

    static const struct
    {
        const char* token;
        ServerBrowserTab tab;
    } kTabs[] = {
        { "#ServerBrowser_InternetTab", ServerBrowserTab::Internet },
        { "#ServerBrowser_FavoritesTab", ServerBrowserTab::Favorites },
        { "#ServerBrowser_HistoryTab", ServerBrowserTab::History },
        { "#ServerBrowser_LanTab", ServerBrowserTab::LAN },
    };

    BeginRow("#GameUI_ServerBrowserInitialTab", m_MiscEdited.serverBrowserTab != m_MiscSaved.serverBrowserTab);
    std::string tabPreview;
    for (const auto& tab : kTabs)
    {
        if ((int)tab.tab == m_MiscEdited.serverBrowserTab)
            tabPreview = Localized(tab.token);
    }
    if (ImGui::BeginCombo("##InitialTab", tabPreview.c_str()))
    {
        for (const auto& tab : kTabs)
        {
            if (ImGui::Selectable(Localized(tab.token).c_str(), (int)tab.tab == m_MiscEdited.serverBrowserTab))
                m_MiscEdited.serverBrowserTab = (int)tab.tab;
        }
        ImGui::EndCombo();
    }
    EndRow();

    BeginRow("#GameUI_DisableAutoOpenServerBrowser", m_MiscEdited.disableAutoOpenServerBrowser != m_MiscSaved.disableAutoOpenServerBrowser);
    ImGui::Checkbox("##DisableAutoOpen", &m_MiscEdited.disableAutoOpenServerBrowser);
    EndRow();
    EndCard();

    // the ImGui windows each keep their VGUI predecessor behind a cvar
    BeginCard("#GameUI_OptionsClassicWindows", "Classic windows");
    ImGui::TextDisabled("%s", Localized("#GameUI_OptionsClassicHint", "The old windows instead of the new ones").c_str());
    CvarCheckbox("#GameUI_OptionsClassicConsole", "con_legacy");
    CvarCheckbox("#GameUI_OptionsClassicBrowser", "sb_legacy");
    CvarCheckbox("#GameUI_OptionsClassicOptions", "opt_legacy");
    EndCard();
}

void CImGuiOptions::DrawVoice()
{
    IVoiceTweak* tweak = engine->pVoiceTweak;

    // while the microphone is tested, the settings stay as the test started with them
    ImGui::BeginDisabled(m_bTestingMicrophone);

    BeginCard("#GameUI_OptionsVoiceChat", "Voice chat");
    CvarCheckbox("#GameUI_EnableVoice", "voice_modenable");
    CvarSlider("#GameUI_VoiceReceiveVolume", "voice_scale", 0.0f, 1.0f, "%.0f%%", 100.0f);
    CvarSlider("#GameUI_VoiceOverdrive", "voice_overdrive", 1.0f, 10.0f, "%.1f");
    EndCard();

    BeginCard("#GameUI_OptionsMicrophone", "Microphone");
    ImGui::BeginDisabled(!tweak);
    BeginRow("#GameUI_VoiceTransmitVolume", m_VoiceEdited.microphoneVolume != m_VoiceSaved.microphoneVolume);
    ImGui::SliderInt("##MicrophoneVolume", &m_VoiceEdited.microphoneVolume, 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp);
    EndRow();

    BeginRow("#GameUI_BoostMicrophone", m_VoiceEdited.microphoneBoost != m_VoiceSaved.microphoneBoost);
    ImGui::Checkbox("##MicrophoneBoost", &m_VoiceEdited.microphoneBoost);
    EndRow();
    ImGui::EndDisabled();

    ImGui::EndDisabled();

    // the test button and the meter work while everything else is locked
    BeginRow(m_bTestingMicrophone ? "#GameUI_StopTestMicrophone" : "#GameUI_TestMicrophone", false);
    ImGui::BeginDisabled(!tweak);
    float meterWidth = ImGui::CalcItemWidth();
    if (ImGui::Button(Localized(m_bTestingMicrophone ? "#GameUI_StopTestMicrophone" : "#GameUI_TestMicrophone").c_str(), ImVec2(meterWidth, 0)))
    {
        if (m_bTestingMicrophone)
            StopMicrophoneTest();
        else
            StartMicrophoneTest();
    }
    ImGui::EndDisabled();
    EndRow();

    if (m_bTestingMicrophone)
    {
        // the speaking volume is a 16-bit sample's size
        float level = std::clamp(tweak->GetSpeakingVolume() / 32768.0f, 0.0f, 1.0f);
        BeginRow("#GameUI_OptionsMicrophoneLevel", false);
        ImGui::ProgressBar(level, ImVec2(meterWidth, ImGui::GetFrameHeight()), "");
        EndRow();
    }
    EndCard();

    if (!tweak)
        ImGui::TextDisabled("%s", Localized("#GameUI_OptionsNoVoiceTweak", "The engine gave no access to the microphone").c_str());

    // the captions with a * point at this
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", Localized("#GameUI_Miles_Voice").c_str());
    ImGui::PopTextWrapPos();
}

// "16:9" for 1920 x 1080; empty when the numbers would say nothing, like 683:384
static std::string AspectRatioText(int width, int height)
{
    // 16:10 reduces to 8:5, which nobody calls it
    if (width * 10 == height * 16)
        return "16:10";

    int divisor = std::gcd(width, height);
    int x = width / divisor, y = height / divisor;
    if (x > 32 || y > 32)
        return {};

    return std::to_string(x) + ":" + std::to_string(y);
}

void CImGuiOptions::DrawVideo()
{
    VideoSettings& video = m_VideoEdited;
    auto toggle = [](const char* id, int& value)
    {
        bool on = value != 0;
        if (ImGui::Checkbox(id, &on))
            value = on ? 1 : 0;
    };

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", Localized("#GameUI_VideoRestart").c_str());
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0, 4));

    BeginCard("#GameUI_DisplayMode", "Display mode");

    BeginRow("#GameUI_Resolution", video.width != m_VideoSaved.width || video.height != m_VideoSaved.height);
    auto modeText = [](int width, int height)
    {
        std::string text = std::to_string(width) + " x " + std::to_string(height);
        std::string aspect = AspectRatioText(width, height);
        return aspect.empty() ? text : text + "   " + aspect;
    };
    if (ImGui::BeginCombo("##Resolution", modeText(video.width, video.height).c_str(), ImGuiComboFlags_HeightLarge))
    {
        for (const auto& [width, height] : m_VideoModes)
        {
            bool selected = width == video.width && height == video.height;
            if (ImGui::Selectable(modeText(width, height).c_str(), selected))
            {
                video.width = width;
                video.height = height;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    EndRow();

    BeginRow("#GameUI_Windowed", video.windowed != m_VideoSaved.windowed);
    toggle("##Windowed", video.windowed);
    EndRow();

    BeginRow("#GameUI_StretchAspect", video.stretchAspect != m_VideoSaved.stretchAspect);
    toggle("##StretchAspect", video.stretchAspect);
    EndRow();

    CvarCheckbox("#GameUI_VSync", "gl_vsync");
    EndCard();

    BeginCard("#GameUI_OptionsPicture", "Picture");
    CvarSlider("#GameUI_Brightness", "brightness", 0.0f, 2.0f, "%.2f");
    CvarSlider("#GameUI_Gamma", "gamma", 1.0f, 3.0f, "%.2f");

    // the text is two lines for the old dialog's narrow label: the caption, then why to use it
    std::string lowDetail = Localized("#GameUI_LowVideoDetail");
    size_t newline = lowDetail.find('\n');
    BeginRowText(lowDetail.substr(0, newline), video.lowDetail != m_VideoSaved.lowDetail);
    toggle("##LowDetail", video.lowDetail);
    if (newline != std::string::npos)
        SetNextRowHintText(lowDetail.substr(newline + 1));
    EndRow();

    BeginRow("#GameUI_DisableMultitexture", video.disableMultitexture != m_VideoSaved.disableMultitexture);
    toggle("##DisableMultitexture", video.disableMultitexture);
    SetNextRowHint("#GameUI_DisableMultitexture_Tooltip");
    EndRow();

    // only mods that ship detail textures get the switch, and only working when the renderer has them
    if (ModInfo().GetDetailedTexture())
    {
        ImGui::BeginDisabled(engine->pfnGetCvarFloat("r_detailtexturessupported") <= 0.0f || video.disableMultitexture);
        CvarCheckbox("#GameUI_DetailTextures", "r_detailtextures");
        ImGui::EndDisabled();
    }
    EndCard();

    BeginCard("#GameUI_OptionsFieldOfView", "Field of view");
    CvarSlider("#GameUI_FovAngle", "fov_angle", 70.0f, 100.0f, "%.0f");
    CvarCheckbox("#GameUI_FovFix", "fov_horplus");

    BeginRow("#GameUI_OptionsViewmodelFovAuto", video.viewmodelFovAuto != m_VideoSaved.viewmodelFovAuto);
    ImGui::Checkbox("##ViewmodelFovAuto", &video.viewmodelFovAuto);
    EndRow();

    // following the main FOV takes its value once either changes here, so that only opening
    // the page doesn't leave a change behind
    if (video.viewmodelFovAuto && engine->pfnGetCvarPointer("viewmodel_fov"))
    {
        if (m_Pending.count("fov_angle") || video.viewmodelFovAuto != m_VideoSaved.viewmodelFovAuto)
            SetPending("viewmodel_fov", PendingString("fov_angle"));
        else
            m_Pending.erase("viewmodel_fov");
    }

    ImGui::BeginDisabled(video.viewmodelFovAuto);
    CvarSlider("#GameUI_FovViewModelAngle", "viewmodel_fov", 70.0f, 100.0f, "%.0f");
    ImGui::EndDisabled();

    CvarSlider("#GameUI_FovLerp", "fov_lerp", 0.0f, 0.8f, "%.2f s");
    EndCard();

    ImGui::TextDisabled("%s", Localized("#GameUI_OptionsSliderTyping", "Ctrl+click a slider to type a value").c_str());
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

void CImGuiOptions::SetNextRowHint(const char* token)
{
    m_RowHint = Localized(token);
}

void CImGuiOptions::SetNextRowHintText(const std::string& text)
{
    m_RowHint = text;
}

void CImGuiOptions::BeginRow(const char* token, bool pending)
{
    BeginRowText(Localized(token), pending);
}

void CImGuiOptions::BeginRowText(const std::string& caption, bool pending)
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

    // a caption longer than its room is cut off and shown whole on hover
    float captionRoom = m_flRowCaptionRight - pos.x;
    ImGui::AlignTextToFramePadding();
    ImGui::PushClipRect(pos, ImVec2(m_flRowCaptionRight, pos.y + ImGui::GetFrameHeight()), true);
    ImGui::TextUnformatted(caption.c_str());
    ImGui::PopClipRect();
    if (ImGui::CalcTextSize(caption.c_str()).x > captionRoom && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", caption.c_str());

    ImGui::SameLine();
    ImGui::SetCursorScreenPos(ImVec2(controlX, pos.y));
    ImGui::SetNextItemWidth(controlWidth);
}

void CImGuiOptions::EndRow()
{
    std::string hint = std::move(m_RowHint);
    m_RowHint.clear();
    if (hint.empty())
        return;

    // the control has moved the cursor below itself; the hint goes under the caption instead
    ImGui::SetCursorScreenPos(ImVec2(m_flRowLeft, m_flRowTop + ImGui::GetFrameHeight()));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (m_flRowCaptionRight - m_flRowLeft));
    ImGui::TextDisabled("%s", hint.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0, 2));
}

bool CImGuiOptions::CvarCheckbox(const char* token, const char* cvar)
{
    if (!engine->pfnGetCvarPointer(cvar))
    {
        m_RowHint.clear();
        return false;
    }

    BeginRow(token, m_Pending.count(cvar) != 0);

    bool value = PendingValue(cvar) != 0.0f;
    ImGui::PushID(cvar);
    bool changed = ImGui::Checkbox("##Value", &value);
    ImGui::PopID();
    EndRow();

    if (changed)
        SetPending(cvar, value ? "1" : "0");
    return changed;
}

bool CImGuiOptions::CvarNegateCheckbox(const char* token, const char* cvar)
{
    if (!engine->pfnGetCvarPointer(cvar))
    {
        m_RowHint.clear();
        return false;
    }

    BeginRow(token, m_Pending.count(cvar) != 0);

    float current = PendingValue(cvar);
    bool negative = current < 0.0f;
    ImGui::PushID(cvar);
    bool changed = ImGui::Checkbox("##Value", &negative);
    ImGui::PopID();
    EndRow();

    if (changed)
    {
        // a zero has no sign to flip, so it starts over from the engine's default
        float magnitude = std::fabs(current);
        if (magnitude < 0.00001f)
            magnitude = 0.022f;

        char value[32];
        snprintf(value, sizeof(value), "%g", negative ? -magnitude : magnitude);
        SetPending(cvar, value);
    }
    return changed;
}

bool CImGuiOptions::KeyToggleCheckbox(const char* token, const char* keyName, const char* command)
{
    bool down;
    if (!g_pGameUIFuncs->IsKeyDown(keyName, down))
    {
        m_RowHint.clear();
        return false;
    }

    auto pending = m_PendingKeys.find(command);
    BeginRow(token, pending != m_PendingKeys.end());

    bool value = pending != m_PendingKeys.end() ? pending->second : down;
    ImGui::PushID(command);
    bool changed = ImGui::Checkbox("##Value", &value);
    ImGui::PopID();
    EndRow();

    if (changed)
    {
        if (value == down)
            m_PendingKeys.erase(command);
        else
            m_PendingKeys[command] = value;
    }
    return changed;
}

bool CImGuiOptions::CvarSlider(const char* token, const char* cvar, float min, float max, const char* format, float displayScale, int flags)
{
    if (!engine->pfnGetCvarPointer(cvar))
    {
        m_RowHint.clear();
        return false;
    }

    BeginRow(token, m_Pending.count(cvar) != 0);

    float shown = PendingValue(cvar) * displayScale;
    ImGui::PushID(cvar);
    bool changed = ImGui::SliderFloat("##Value", &shown, min * displayScale, max * displayScale, format, ImGuiSliderFlags_AlwaysClamp | flags);
    ImGui::PopID();
    EndRow();

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
    {
        m_RowHint.clear();
        return false;
    }

    BeginRow(token, m_Pending.count(cvar) != 0);

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
    EndRow();

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

size_t CImGuiOptions::PendingCount() const
{
    size_t misc = (m_MiscEdited.scheme != m_MiscSaved.scheme)
        + (m_MiscEdited.serverBrowserTab != m_MiscSaved.serverBrowserTab)
        + (m_MiscEdited.disableAutoOpenServerBrowser != m_MiscSaved.disableAutoOpenServerBrowser);
    size_t voice = (m_VoiceEdited.microphoneVolume != m_VoiceSaved.microphoneVolume)
        + (m_VoiceEdited.microphoneBoost != m_VoiceSaved.microphoneBoost);
    const VideoSettings& a = m_VideoEdited;
    const VideoSettings& b = m_VideoSaved;
    size_t video = (a.width != b.width || a.height != b.height) + (a.windowed != b.windowed) + (a.lowDetail != b.lowDetail)
        + (a.disableMultitexture != b.disableMultitexture) + (a.stretchAspect != b.stretchAspect) + (a.viewmodelFovAuto != b.viewmodelFovAuto);

    return m_Pending.size() + m_PendingKeys.size() + misc + voice + video;
}

void CImGuiOptions::LoadMiscSettings()
{
    KeyValues* settings = OptionsSubMiscellaneous::GetSettings();
    m_MiscSaved.scheme = settings->GetString(OptionsSubMiscellaneous::kSchemeKey, OptionsSubMiscellaneous::kDefaultScheme);
    m_MiscSaved.serverBrowserTab = settings->GetInt(OptionsSubMiscellaneous::kServerBrowserInitialTabKey, (int)ServerBrowserTab::Internet);
    m_MiscSaved.disableAutoOpenServerBrowser = settings->GetBool(OptionsSubMiscellaneous::kDisableAutoOpenServerBrowserKey);
    settings->deleteThis();
    m_MiscEdited = m_MiscSaved;

    m_Schemes.clear();
    FileFindHandle_t handle = 0;
    for (const char* file = g_pFullFileSystem->FindFirst("resource/schemes/*.res", &handle, "GAME"); file; file = g_pFullFileSystem->FindNext(handle))
    {
        if (!g_pFullFileSystem->FindIsDirectory(handle))
            m_Schemes.push_back(file);
    }
    g_pFullFileSystem->FindClose(handle);
}

void CImGuiOptions::SaveMiscSettings()
{
    if (m_MiscEdited == m_MiscSaved)
        return;

    KeyValues* settings = OptionsSubMiscellaneous::GetSettings();
    settings->SetString(OptionsSubMiscellaneous::kSchemeKey, m_MiscEdited.scheme.c_str());
    settings->SetInt(OptionsSubMiscellaneous::kServerBrowserInitialTabKey, m_MiscEdited.serverBrowserTab);
    settings->SetBool(OptionsSubMiscellaneous::kDisableAutoOpenServerBrowserKey, m_MiscEdited.disableAutoOpenServerBrowser);
    settings->SaveToFile(g_pFullFileSystem, OptionsSubMiscellaneous::kUserSaveDataPath, "GAMECONFIG");
    settings->deleteThis();

    bool schemeChanged = m_MiscEdited.scheme != m_MiscSaved.scheme;
    m_MiscSaved = m_MiscEdited;

    // VGUI loads its scheme once, so a new one needs the game restarted, as the old dialog did
    if (schemeChanged)
    {
        engine->pfnClientCmd("fmod stop\n");
        engine->pfnClientCmd("_restart\n");
    }
}

void CImGuiOptions::ApplyChanges()
{
    // stopping restores what the test changed, so it has to come before the new values
    StopMicrophoneTest();

    bool restartForCvars = m_Pending.count("brightness") || m_Pending.count("gamma");

    for (const auto& [cvar, value] : m_Pending)
        engine->Cvar_Set(cvar.c_str(), value.c_str());

    for (const auto& [command, on] : m_PendingKeys)
    {
        char text[64];
        snprintf(text, sizeof(text), "%c%s\n", on ? '+' : '-', command.c_str());
        engine->pfnClientCmd(text);
    }

    m_Pending.clear();
    m_PendingKeys.clear();
    SaveMiscSettings();
    SaveVoiceSettings();
    // last, since it may restart the game
    SaveVideoSettings(restartForCvars);
}

void CImGuiOptions::LoadVideoSettings()
{
    VideoSettings& video = m_VideoSaved;
    g_pGameUIFuncs->GetCurrentVideoMode(&video.width, &video.height, &video.bpp);

    char renderer[128] = {};
    g_pGameUIFuncs->GetCurrentRenderer(renderer, sizeof(renderer), &video.windowed, &video.hdModels, &video.addonsFolder, &video.lowDetail);
    video.renderer = renderer;

    nitro_utils::FileConfigProvider config("user_game_config.ini");
    video.disableMultitexture = config.get_value_int("disable_multitexture", 0);
    video.stretchAspect = config.get_value_int("stretch_aspect", 0);

    KeyValues* advanced = CVideoAdvancedDialog::GetSettings();
    video.viewmodelFovAuto = advanced->GetBool(CVideoAdvancedDialog::kFovViewmodelAutoCheckboxKey, true);
    advanced->deleteThis();

    m_VideoEdited = m_VideoSaved;

    // the modes the old dialog offered: none smaller than 640x480
    m_VideoModes.clear();
    vmode_t* modes = nullptr;
    int count = 0;
    g_pGameUIFuncs->GetVideoModes(&modes, &count);
    for (int i = 0; i < count; i++)
    {
        if (modes[i].iWidth >= 640 && modes[i].iHeight >= 480)
            m_VideoModes.emplace_back(modes[i].iWidth, modes[i].iHeight);
    }
}

void CImGuiOptions::SaveVideoSettings(bool restartForCvars)
{
    VideoSettings& video = m_VideoEdited;

    if (video.viewmodelFovAuto != m_VideoSaved.viewmodelFovAuto)
    {
        KeyValues* advanced = CVideoAdvancedDialog::GetSettings();
        advanced->SetBool(CVideoAdvancedDialog::kFovViewmodelAutoCheckboxKey, video.viewmodelFovAuto);
        advanced->SaveToFile(g_pFullFileSystem, CVideoAdvancedDialog::kUserSaveDataPath, "GAMECONFIG");
        advanced->deleteThis();
        m_VideoSaved.viewmodelFovAuto = video.viewmodelFovAuto;
    }

    if (video == m_VideoSaved && !restartForCvars)
        return;

    // the same commands the old dialog sent; the engine reads them back when it restarts
    char command[256];
    snprintf(command, sizeof(command), "_setvideomode %i %i %i\n", video.width, video.height, video.bpp);
    engine->pfnClientCmd(command);
    snprintf(command, sizeof(command), "_setrenderer %s %s\n", video.renderer.c_str(), video.windowed ? "windowed" : "fullscreen");
    engine->pfnClientCmd(command);
    snprintf(command, sizeof(command), "_sethdmodels %d\n", video.hdModels);
    engine->pfnClientCmd(command);
    snprintf(command, sizeof(command), "_setaddons_folder %d\n", video.addonsFolder);
    engine->pfnClientCmd(command);
    snprintf(command, sizeof(command), "_set_vid_level %d\n", video.lowDetail);
    engine->pfnClientCmd(command);

    nitro_utils::FileConfigProvider config("user_game_config.ini");
    config.set_value("", "disable_multitexture", std::to_string(video.disableMultitexture), true);
    config.set_value("", "stretch_aspect", std::to_string(video.stretchAspect), true);

    m_VideoSaved = video;

    engine->pfnClientCmd("fmod stop\n");
    engine->pfnClientCmd("_restart\n");
}

void CImGuiOptions::LoadVoiceSettings()
{
    IVoiceTweak* tweak = engine->pVoiceTweak;
    if (!tweak)
        return;

    // the old dialog did this too: the tweak's own copy of voice_scale may be stale
    tweak->SetControlFloat(OtherSpeakerScale, engine->pfnGetCvarFloat("voice_scale"));

    m_VoiceSaved.microphoneVolume = static_cast<int>(std::round(tweak->GetControlFloat(MicrophoneVolume) * 100.0f));
    m_VoiceSaved.microphoneBoost = tweak->GetControlFloat(MicBoost) != 0.0f;
    m_VoiceEdited = m_VoiceSaved;
}

void CImGuiOptions::SaveVoiceSettings()
{
    IVoiceTweak* tweak = engine->pVoiceTweak;
    if (!tweak || m_VoiceEdited == m_VoiceSaved)
        return;

    tweak->SetControlFloat(MicrophoneVolume, m_VoiceEdited.microphoneVolume / 100.0f);
    tweak->SetControlFloat(MicBoost, m_VoiceEdited.microphoneBoost ? 1.0f : 0.0f);
    m_VoiceSaved = m_VoiceEdited;
}

void CImGuiOptions::StartMicrophoneTest()
{
    IVoiceTweak* tweak = engine->pVoiceTweak;
    if (!tweak || m_bTestingMicrophone)
        return;

    // the test plays what the page shows, applied or not
    tweak->SetControlFloat(MicrophoneVolume, m_VoiceEdited.microphoneVolume / 100.0f);
    tweak->SetControlFloat(MicBoost, m_VoiceEdited.microphoneBoost ? 1.0f : 0.0f);
    m_VoiceScaleBeforeTest = engine->pfnGetCvarString("voice_scale");
    engine->Cvar_Set("voice_scale", PendingString("voice_scale").c_str());

    m_bTestingMicrophone = true;
    if (!tweak->StartVoiceTweakMode())
        StopMicrophoneTest();
}

void CImGuiOptions::StopMicrophoneTest()
{
    IVoiceTweak* tweak = engine->pVoiceTweak;
    if (!tweak || !m_bTestingMicrophone)
        return;

    m_bTestingMicrophone = false;
    tweak->EndVoiceTweakMode();

    tweak->SetControlFloat(MicrophoneVolume, m_VoiceSaved.microphoneVolume / 100.0f);
    tweak->SetControlFloat(MicBoost, m_VoiceSaved.microphoneBoost ? 1.0f : 0.0f);
    engine->Cvar_Set("voice_scale", m_VoiceScaleBeforeTest.c_str());
}
