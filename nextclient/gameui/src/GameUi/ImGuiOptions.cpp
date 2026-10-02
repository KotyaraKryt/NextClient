#include "ImGuiOptions.h"
#include "BasePanel.h"
#include "GameUi.h"
#include "IGameUIFuncs.h"
#include "ivoicetweak.h"
#include "OptionsDialog/LogoFile.h"
#include "OptionsDialog/VideoAdvancedDialog.h"
#include "ScriptObject.h"
#include "Controls/BobPreviewPanel.h"

#include <crosshair/crosshair.h>
#include <cvars/cvar_defaults.h>
#include <view/view_bob.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include "utils/bmp_compat.h"
#endif
#include <GL/gl.h>
#include <vgui/ISurfaceNext.h>

#include <nitro_utils/config/FileConfigProvider.h>
#include "ModInfo.h"
#include "OptionsDialog/OptionsSubMiscellaneous.h"

#include <FileSystem.h>
#include <KeyValues.h>

#include <cvardef.h>
#include <tier1/strtools.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <functional>
#include <numeric>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace ImGuiForm;

namespace
{
    cvar_t* g_pLegacyCvar = nullptr;

    constexpr float kSidebarWidth = 190.0f;
    constexpr float kPagePadding = 18.0f;

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
}

namespace
{
    // the colors the old dialog offered; cl_logocolor holds the name
    struct SprayColor
    {
        const char* name;
        int r, g, b;
    };

    const SprayColor kSprayColors[] = {
        { "#Valve_Orange", 255, 120, 24 },
        { "#Valve_Yellow", 225, 180, 24 },
        { "#Valve_Blue", 0, 60, 255 },
        { "#Valve_Ltblue", 0, 167, 255 },
        { "#Valve_Green", 0, 167, 0 },
        { "#Valve_Red", 255, 43, 0 },
        { "#Valve_Brown", 123, 73, 0 },
        { "#Valve_Ltgray", 100, 100, 100 },
        { "#Valve_Dkgray", 36, 36, 36 },
    };

    const SprayColor& SprayColorByName(const std::string& name)
    {
        for (const SprayColor& color : kSprayColors)
        {
            if (!V_stricmp(color.name, name.c_str()))
                return color;
        }
        return kSprayColors[0];
    }

    std::string ReadFile(const char* path)
    {
        std::string data;
        FileHandle_t file = g_pFullFileSystem->Open(path, "rb");
        if (file == FILESYSTEM_INVALID_HANDLE)
            return data;

        data.resize(g_pFullFileSystem->Size(file));
        g_pFullFileSystem->Read(data.data(), static_cast<int>(data.size()), file);
        g_pFullFileSystem->Close(file);
        return data;
    }

    uint32_t ReadU32(const std::string& data, size_t at)
    {
        uint32_t value;
        memcpy(&value, data.data() + at, sizeof(value));
        return value;
    }

    // A spray is an 8-bit BMP whose palette doesn't matter: the engine paints each pixel in the chosen
    // color, darker by its index. This gives the DIB that UpdateLogoWAD takes (the file after its
    // 14-byte header, with the palette remapped like that), empty if the file isn't such a BMP
    std::string LoadSprayDib(const std::string& logo, const SprayColor& color, int& width, int& height)
    {
        std::string file = ReadFile(("logos/" + logo + ".bmp").c_str());
        constexpr size_t kFileHeader = 14, kInfoHeader = 40, kPalette = 256 * 4;
        if (file.size() < kFileHeader + kInfoHeader + kPalette || file[0] != 'B' || file[1] != 'M')
            return {};

        int32_t w, h;
        uint16_t bits;
        memcpy(&w, file.data() + 18, 4);
        memcpy(&h, file.data() + 22, 4);
        memcpy(&bits, file.data() + 28, 2);
        // UpdateLogoWAD expects the pixels right after the palette, as Valve's tools write them
        if (bits != 8 || w <= 0 || h <= 0 || ReadU32(file, 10) != kFileHeader + kInfoHeader + kPalette
            || file.size() < kFileHeader + kInfoHeader + kPalette + size_t((w + 3) & ~3) * h)
            return {};

        std::string dib = file.substr(kFileHeader);
        for (int i = 0; i < 256; i++)
        {
            float t = i / 256.0f;
            unsigned char* entry = reinterpret_cast<unsigned char*>(dib.data()) + kInfoHeader + i * 4;
            entry[0] = static_cast<unsigned char>(color.b * t);
            entry[1] = static_cast<unsigned char>(color.g * t);
            entry[2] = static_cast<unsigned char>(color.r * t);
            entry[3] = 0;
        }

        width = w;
        height = h;
        return dib;
    }
}

CImGuiOptions::CImGuiOptions() : BaseClass("options_layout.ini")
{
    SetVisible(false);

    // the old dialog's pages, sorted into the sidebar's groups
    bool singlePlayerOnly = ModInfo().IsSinglePlayerOnly();
    if (!singlePlayerOnly)
        m_Pages.push_back({ "multiplayer", "#GameUI_Multiplayer", kGroupPlayer, &CImGuiOptions::DrawMultiplayer, "#GameUI_OptionsMultiplayerHint", "Your name, spray and what servers see of you" });
    m_Pages.push_back({ "game", "#GameUI_Game", kGroupPlayer, &CImGuiOptions::DrawGame, "#GameUI_OptionsGameHint", "Crosshair and how the weapon and the view move" });
    m_Pages.push_back({ "keyboard", "#GameUI_Keyboard", kGroupControls, &CImGuiOptions::DrawKeyboard, "#GameUI_OptionsKeyboardHint", "Click a key to change it, right-click to clear it, Esc to stop" });
    m_Pages.push_back({ "mouse", "#GameUI_Mouse", kGroupControls, &CImGuiOptions::DrawMouse, "#GameUI_OptionsMouseHint", "Sensitivity, looking around and the joystick" });
    m_Pages.push_back({ "audio", "#GameUI_Audio", kGroupSystem, &CImGuiOptions::DrawAudio, "#GameUI_OptionsAudioHint", "Volume and sound quality" });
    m_Pages.push_back({ "video", "#GameUI_Video", kGroupSystem, &CImGuiOptions::DrawVideo, "#GameUI_OptionsVideoHint", "Screen, picture and field of view" });
    if (!singlePlayerOnly)
        m_Pages.push_back({ "voice", "#GameUI_Voice", kGroupSystem, &CImGuiOptions::DrawVoice, "#GameUI_OptionsVoiceHint", "Voice chat and the microphone" });
    m_Pages.push_back({ "miscellaneous", "#GameUI_Miscellaneous", kGroupSystem, &CImGuiOptions::DrawMisc, "#GameUI_OptionsMiscHint", "Look of the menus and the server browser" });

    m_SetInfoKeys.insert("_pw");

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
        LoadMultiplayerSettings();
        LoadBindings();
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
    m_iCaptureRow = -1;
    m_Bindings = m_BindingsSaved;
    if (m_hGamePreview.Get())
        m_hGamePreview->SetVisible(false);

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

    m_bPreviewDrawn = false;

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

    if (m_hGamePreview.Get() && !m_bPreviewDrawn)
        m_hGamePreview->SetVisible(false);

    if (!open)
        Close();
}

void CImGuiOptions::DrawPageList()
{
    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);

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

        if (ListItem(page.id, Localized(page.token), m_pSelected == &page))
            m_pSelected = &page;

        if (!page.draw)
        {
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImVec2 markPos(max.x - legacyMarkWidth - 10.0f, min.y + (max.y - min.y - ImGui::GetTextLineHeight()) * 0.5f);
            ImGui::GetWindowDrawList()->AddText(markPos, ImGui::GetColorU32(ImGuiCol_TextDisabled), legacyMark);
            if (ImGui::IsItemHovered())
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
    CvarCheckbox("#GameUI_OptionsClassicCreateServer", "newgame_legacy");
    CvarCheckbox("#GameUI_OptionsClassicLoading", "loading_legacy");
    CvarCheckbox("#GameUI_OptionsClassicPlayerList", "plist_legacy");
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

void CImGuiOptions::DrawMultiplayer()
{
    BeginCard("#GameUI_OptionsGroupPlayer", "Player");
    CvarText("#GameUI_PlayerName", "name");
    CvarText("#GameUI_AdminPassword", "_pw", true);
    EndCard();

    DrawSpray();

    BeginCard("#GameUI_OptionsCrosshair", "Crosshair");
    BeginRow("#GameUI_OptionsCrosshairWhere", false);
    if (ImGui::Button(Localized("#GameUI_CrosshairSettingsBtn").c_str(), ImVec2(ImGui::CalcItemWidth(), 0)))
        ShowCrosshairSettings();
    EndRow();
    EndCard();

    if (m_pAdvancedOptions && m_pAdvancedOptions->pObjList)
    {
        BeginCard("#GameUI_MultiplayerAdvanced", "Advanced");
        for (CScriptObject* option = m_pAdvancedOptions->pObjList; option; option = option->pNext)
            DrawAdvancedOption(*option);
        EndCard();
    }
}

void CImGuiOptions::DrawSpray()
{
    if (m_Logos.empty())
        return;

    BeginCard("#GameUI_SpraypaintImage", "Spraypaint image");
    ImVec2 cardTop = ImGui::GetCursorScreenPos();

    // a spray picked from a name that isn't in logos/ any more falls back to the first one
    std::string logo = PendingString("cl_logofile");
    if (std::find(m_Logos.begin(), m_Logos.end(), logo) == m_Logos.end())
        logo = m_Logos.front();

    BeginRow("#GameUI_OptionsSpray", m_Pending.count("cl_logofile") != 0);
    if (ImGui::BeginCombo("##Spray", logo.c_str(), ImGuiComboFlags_HeightLarge))
    {
        for (const std::string& name : m_Logos)
        {
            if (ImGui::Selectable(name.c_str(), name == logo))
                SetPending("cl_logofile", name);
        }
        ImGui::EndCombo();
    }
    EndRow();

    const SprayColor& color = SprayColorByName(PendingString("cl_logocolor"));
    BeginRow("#GameUI_OptionsSprayColor", m_Pending.count("cl_logocolor") != 0);
    if (ImGui::BeginCombo("##SprayColor", Localized(color.name, color.name + 1).c_str()))
    {
        for (const SprayColor& choice : kSprayColors)
        {
            ImVec4 swatch(choice.r / 255.0f, choice.g / 255.0f, choice.b / 255.0f, 1.0f);
            ImGui::ColorButton(choice.name, swatch, ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()));
            ImGui::SameLine();
            if (ImGui::Selectable(Localized(choice.name, choice.name + 1).c_str(), &choice == &color))
                SetPending("cl_logocolor", choice.name);
        }
        ImGui::EndCombo();
    }
    EndRow();

    // the preview, remade only when the spray or its color changes
    std::string key = logo + "|" + color.name;
    if (key != m_LogoTextureKey)
    {
        m_LogoTextureKey = key;
        int width = 0, height = 0;
        std::string dib = LoadSprayDib(logo, color, width, height);
        m_iLogoWidth = m_iLogoHeight = 0;
        if (!dib.empty())
        {
            // the DIB's rows go bottom up and are padded to 4 bytes
            const unsigned char* pixels = reinterpret_cast<const unsigned char*>(dib.data()) + 40 + 256 * 4;
            int stride = (width + 3) & ~3;
            std::vector<unsigned char> rgba(size_t(width) * height * 4);
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    float t = pixels[size_t(height - 1 - y) * stride + x] / 256.0f;
                    unsigned char* out = &rgba[(size_t(y) * width + x) * 4];
                    out[0] = static_cast<unsigned char>(color.r * t);
                    out[1] = static_cast<unsigned char>(color.g * t);
                    out[2] = static_cast<unsigned char>(color.b * t);
                    out[3] = 255;
                }
            }

            // a texture name from the engine's counter, like the font atlas's
            if (!m_iLogoTexture)
                m_iLogoTexture = vgui2::surface()->CreateNewTextureID();

            GLint lastTexture;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);
            glBindTexture(GL_TEXTURE_2D, m_iLogoTexture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glBindTexture(GL_TEXTURE_2D, lastTexture);

            m_iLogoWidth = width;
            m_iLogoHeight = height;
        }
    }

    if (m_iLogoWidth > 0)
    {
        // whole pixels, so the nearest filter keeps the spray sharp
        float side = ImGui::GetFrameHeight() * 2.0f + ImGui::GetStyle().ItemSpacing.y;
        float scale = std::max(1.0f, std::floor(side / std::max(m_iLogoWidth, m_iLogoHeight)));
        if (std::max(m_iLogoWidth, m_iLogoHeight) > side)
            scale = side / std::max(m_iLogoWidth, m_iLogoHeight);
        ImVec2 size(m_iLogoWidth * scale, m_iLogoHeight * scale);

        // beside the two combo boxes, in the gap between their captions and them
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        float controlWidth = std::clamp((m_flCardRight - cardTop.x) * 0.5f, 180.0f, 340.0f);
        ImGui::SetCursorScreenPos(ImVec2(m_flCardRight - controlWidth - ImGui::GetStyle().ItemSpacing.x * 2.0f - size.x, cardTop.y));
        ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(m_iLogoTexture)), size);
        ImGui::SetCursorScreenPos(cursor);
    }

    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (m_flCardRight - ImGui::GetCursorScreenPos().x));
    ImGui::TextDisabled("%s", Localized("#GameUI_SpraypaintServerNote").c_str());
    ImGui::PopTextWrapPos();
    EndCard();
}

void CImGuiOptions::DrawAdvancedOption(CScriptObject& option)
{
    const char* cvar = option.cvarname;
    std::string value = PendingString(cvar);
    if (ScriptOptionRow(option, value, m_Pending.count(cvar) != 0))
        SetPending(cvar, value);
}

namespace
{
    struct GameTab
    {
        const char* token;
        PreviewMove move;
        PreviewDemo demo;
    };

    // the old page's tabs, each with the movement and demo that make its settings show
    const GameTab kGameTabs[] = {
        { "#GameUI_GameTabCrosshair", PreviewMove::kIdle, PreviewDemo::kNone },
        { "#GameUI_GameTabBobbing", PreviewMove::kRun, PreviewDemo::kNone },
        { "#GameUI_GameTabModel", PreviewMove::kIdle, PreviewDemo::kNone },
        { "#GameUI_GameTabInertia", PreviewMove::kIdle, PreviewDemo::kLag },
        { "#GameUI_GameTabCamera", PreviewMove::kStrafe, PreviewDemo::kWeaponSwitch },
    };

    enum
    {
        kTabCrosshair,
        kTabBobbing,
        kTabModel,
        kTabInertia,
        kTabCamera,
    };

    const char* const kPreviewMoves[] = {
        "#GameUI_BobPreviewRun",
        "#GameUI_BobPreviewWalk",
        "#GameUI_BobPreviewStrafe",
        "#GameUI_BobPreviewIdle",
    };

    // each tab's cvars, which Defaults puts back
    const char* const kTabCvars[][8] = {
        { cvars::kCrosshairType.name, cvars::kCrosshairSize.name, cvars::kCrosshairColor.name, cvars::kCrosshairTranslucent.name, cvars::kDynamicCrosshair.name },
        { cvars::kBobStyle.name, cvars::kBob.name, cvars::kBobCycle.name, cvars::kBobUp.name, cvars::kBobAmtVert.name, cvars::kBobAmtLat.name, cvars::kBobLowerAmt.name },
        { cvars::kViewmodelOffsetX.name, cvars::kViewmodelOffsetY.name, cvars::kViewmodelOffsetZ.name, cvars::kViewmodelFov.name, cvars::kViewmodelDisableShift.name },
        { cvars::kViewmodelLagStyle.name, cvars::kViewmodelLagScale.name, cvars::kViewmodelLagSpeed.name },
        { cvars::kRollAngle.name, cvars::kRollSpeed.name, cvars::kCameraMovementScale.name, cvars::kCameraMovementInterp.name, cvars::kBobCamera.name },
    };

    // "r g b" as cl_crosshair_color carries it
    ncl_math::Color ParseCrosshairColor(const std::string& text)
    {
        int r, g, b;
        if (sscanf(text.c_str(), "%d %d %d", &r, &g, &b) != 3 && sscanf(cvars::kCrosshairColor.value, "%d %d %d", &r, &g, &b) != 3)
            return {};

        return { (uint8_t)std::clamp(r, 0, 255), (uint8_t)std::clamp(g, 0, 255), (uint8_t)std::clamp(b, 0, 255) };
    }
}

void CImGuiOptions::ShowCrosshairSettings()
{
    for (const Page& page : m_Pages)
    {
        if (!strcmp(page.id, "game"))
            m_pSelected = &page;
    }

    m_iGameTab = kTabCrosshair;
    m_bSelectGameTab = true;
}

void CImGuiOptions::ResetToDefault(const char* cvar)
{
    if (const char* value = cvars::FindDefault(cvar))
        SetPending(cvar, value);
}

void CImGuiOptions::SetPendingFloat(const char* cvar, float value)
{
    char text[32];
    snprintf(text, sizeof(text), "%g", value);
    SetPending(cvar, text);
}

void CImGuiOptions::SyncGamePreview()
{
    CBobPreviewPanel* preview = m_hGamePreview.Get();

    view_bob::BobParams bob;
    bob.style = static_cast<int>(PendingValue(cvars::kBobStyle.name));
    bob.bob = PendingValue(cvars::kBob.name);
    bob.bob_cycle = PendingValue(cvars::kBobCycle.name);
    bob.bob_up = PendingValue(cvars::kBobUp.name);
    bob.amt_vert = PendingValue(cvars::kBobAmtVert.name);
    bob.amt_lat = PendingValue(cvars::kBobAmtLat.name);
    bob.lower_amt = PendingValue(cvars::kBobLowerAmt.name);
    bob.camera_bob = PendingValue(cvars::kBobCamera.name) != 0.0f;
    preview->SetBobParams(bob);

    ViewTuningParams tuning;
    tuning.offset_x = PendingValue(cvars::kViewmodelOffsetX.name);
    tuning.offset_y = PendingValue(cvars::kViewmodelOffsetY.name);
    tuning.offset_z = PendingValue(cvars::kViewmodelOffsetZ.name);
    tuning.disable_shift = PendingValue(cvars::kViewmodelDisableShift.name) != 0.0f;
    tuning.viewmodel_fov = PendingValue(cvars::kViewmodelFov.name);
    tuning.lag_style = static_cast<int>(PendingValue(cvars::kViewmodelLagStyle.name));
    tuning.lag_scale = PendingValue(cvars::kViewmodelLagScale.name);
    tuning.lag_speed = PendingValue(cvars::kViewmodelLagSpeed.name);
    tuning.roll_angle = PendingValue(cvars::kRollAngle.name);
    tuning.roll_speed = PendingValue(cvars::kRollSpeed.name);
    tuning.camera_move_scale = PendingValue(cvars::kCameraMovementScale.name);
    tuning.camera_move_interp = PendingValue(cvars::kCameraMovementInterp.name);
    preview->SetViewTuning(tuning);

    CrosshairParams crosshair;
    crosshair.type = std::clamp(static_cast<int>(PendingValue(cvars::kCrosshairType.name)), 0, crosshair::kTypeCount - 1);
    crosshair.color = ParseCrosshairColor(PendingString(cvars::kCrosshairColor.name));
    crosshair.size_index = crosshair::SizeIndex(PendingString(cvars::kCrosshairSize.name).c_str());
    crosshair.translucent = PendingValue(cvars::kCrosshairTranslucent.name) != 0.0f;
    crosshair.dynamic = PendingValue(cvars::kDynamicCrosshair.name) != 0.0f;
    preview->SetCrosshairParams(crosshair);
}

void CImGuiOptions::DrawGame()
{
    // the preview is the old page's VGUI panel: it draws the scene through the engine, which
    // ImGui can't, so it sits over a hole the page leaves for it
    if (!m_hGamePreview.Get())
    {
        m_hGamePreview = new CBobPreviewPanel(this, "GamePreview");
        m_hGamePreview->SetMouseInputEnabled(false);
        m_hGamePreview->SetKeyBoardInputEnabled(false);
        m_iShownPreviewMove = m_iShownPreviewDemo = -1;
    }

    float previewHeight = std::clamp(ImGui::GetContentRegionAvail().y * 0.48f, 140.0f, 320.0f);
    DrawGamePreview(previewHeight);
    ImGui::Dummy(ImVec2(0, 4));

    int shownTab = m_iGameTab;
    if (ImGui::BeginTabBar("GameTabs"))
    {
        for (int i = 0; i < (int)std::size(kGameTabs); i++)
        {
            ImGuiTabItemFlags flags = m_bSelectGameTab && i == m_iGameTab ? ImGuiTabItemFlags_SetSelected : 0;
            std::string label = Localized(kGameTabs[i].token) + "###GameTab" + std::to_string(i);
            if (ImGui::BeginTabItem(label.c_str(), nullptr, flags))
            {
                shownTab = i;
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    m_bSelectGameTab = false;

    // a tab that comes up starts the movement that shows its settings, as on the old page
    if (shownTab != m_iGameTab)
        m_iPreviewMove = static_cast<int>(kGameTabs[shownTab].move);
    m_iGameTab = shownTab;

    // the settings scroll under the preview, which has to stay where the page left room for it
    ImGui::BeginChild("GameSettings", ImVec2(0, 0), false);
    BeginCard(nullptr, nullptr);
    switch (m_iGameTab)
    {
        case kTabCrosshair: DrawCrosshairTab(); break;
        case kTabBobbing: DrawBobbingTab(); break;
        case kTabModel: DrawModelTab(); break;
        case kTabInertia: DrawInertiaTab(); break;
        case kTabCamera: DrawCameraTab(); break;
    }
    EndCard();
    ImGui::EndChild();

    SyncGamePreview();
}

void CImGuiOptions::DrawGamePreview(float height)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    CBobPreviewPanel* preview = m_hGamePreview.Get();

    struct Preset
    {
        const char* token;
        std::function<void()> apply;
    };

    auto reset = [this](std::initializer_list<const char*> names)
    {
        for (const char* name : names)
            ResetToDefault(name);
    };

    std::vector<Preset> presets;
    switch (m_iGameTab)
    {
        case kTabCrosshair:
            presets = {
                { "#GameUI_PresetClassic", [&] { for (const char* cvar : kTabCvars[kTabCrosshair]) if (cvar) ResetToDefault(cvar); } },
                { "#GameUI_PresetCrosshairDot", [&] { SetPendingFloat(cvars::kCrosshairType.name, crosshair::kTypeDot); SetPending(cvars::kDynamicCrosshair.name, "0"); } },
                { "#GameUI_PresetCrosshairStatic", [&] { SetPending(cvars::kDynamicCrosshair.name, "0"); } },
            };
            break;
        case kTabBobbing:
            presets = {
                { "#GameUI_PresetClassic", [&] { SetPendingFloat(cvars::kBobStyle.name, view_bob::kStyleClassic); reset({ cvars::kBob.name, cvars::kBobCycle.name, cvars::kBobUp.name }); } },
                { "#GameUI_PresetModern", [&] { SetPendingFloat(cvars::kBobStyle.name, view_bob::kStyleModern); reset({ cvars::kBobCycle.name, cvars::kBobUp.name, cvars::kBobAmtVert.name, cvars::kBobAmtLat.name, cvars::kBobLowerAmt.name }); } },
                // every amplitude and not the style, so the weapon stands still in either style
                { "#GameUI_PresetNone", [&] { for (const char* cvar : { cvars::kBob.name, cvars::kBobAmtVert.name, cvars::kBobAmtLat.name, cvars::kBobLowerAmt.name }) SetPending(cvar, "0"); } },
            };
            break;
        case kTabModel:
            presets = {
                { "#GameUI_PresetDefault", [&] { reset({ cvars::kViewmodelOffsetX.name, cvars::kViewmodelOffsetY.name, cvars::kViewmodelOffsetZ.name, cvars::kViewmodelFov.name }); } },
                { "#GameUI_PresetModelCentered", [&] { SetPendingFloat(cvars::kViewmodelOffsetX.name, -1.5f); SetPendingFloat(cvars::kViewmodelOffsetY.name, 1.0f); SetPendingFloat(cvars::kViewmodelOffsetZ.name, 0.5f); } },
                { "#GameUI_PresetModelWide", [&] { SetPendingFloat(cvars::kViewmodelFov.name, 100.0f); } },
            };
            break;
        case kTabInertia:
            presets = {
                { "#GameUI_PresetOff", [&] { SetPending(cvars::kViewmodelLagStyle.name, "0"); } },
                { "#GameUI_ViewLagHL2", [&] { SetPending(cvars::kViewmodelLagStyle.name, "1"); reset({ cvars::kViewmodelLagScale.name, cvars::kViewmodelLagSpeed.name }); } },
                { "#GameUI_ViewLagCSS", [&] { SetPending(cvars::kViewmodelLagStyle.name, "2"); reset({ cvars::kViewmodelLagScale.name }); } },
            };
            break;
        case kTabCamera:
            presets = {
                { "#GameUI_PresetCameraCalm", [&] { reset({ cvars::kRollAngle.name, cvars::kCameraMovementInterp.name }); SetPending(cvars::kCameraMovementScale.name, "0"); } },
                { "#GameUI_PresetCameraQuake", [&] { SetPending(cvars::kRollAngle.name, "2"); reset({ cvars::kRollSpeed.name }); } },
                { "#GameUI_PresetCameraCinematic", [&] { SetPendingFloat(cvars::kCameraMovementScale.name, 1.5f); SetPendingFloat(cvars::kCameraMovementInterp.name, 0.1f); } },
            };
            break;
    }

    // the scene, framed like the player's screen and as big as the page allows
    float avail = ImGui::GetContentRegionAvail().x;
    float aspect = CBobPreviewPanel::get_screen_aspect();
    float width = std::min(height * aspect, avail);
    height = width / aspect;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width) * 0.5f);
    ImVec2 min = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(width, height));
    ImGui::GetWindowDrawList()->AddRect(ImVec2(min.x - 1, min.y - 1), ImVec2(min.x + width + 1, min.y + height + 1),
        ImGui::GetColorU32(ImGuiCol_Border), 2.0f, 0, 2.0f);

    // ImGui works in screen space, while this panel shrinks itself around its windows and a
    // child is placed relative to it
    int x = static_cast<int>(min.x), y = static_cast<int>(min.y);
    ScreenToLocal(x, y);
    preview->SetBounds(x, y, static_cast<int>(width), static_cast<int>(height));
    preview->SetVisible(true);
    m_bPreviewDrawn = true;

    if (m_iShownPreviewMove != m_iPreviewMove)
    {
        m_iShownPreviewMove = m_iPreviewMove;
        preview->SetMoveMode(static_cast<PreviewMove>(m_iPreviewMove));
    }
    int demo = static_cast<int>(kGameTabs[m_iGameTab].demo);
    if (m_iShownPreviewDemo != demo)
    {
        m_iShownPreviewDemo = demo;
        preview->SetDemo(kGameTabs[m_iGameTab].demo);
    }

    ImGui::Dummy(ImVec2(0, 2));

    // under it, how the preview moves on the left and the tab's presets on the right
    auto buttonWidth = [&](const std::string& text) { return ImGui::CalcTextSize(text.c_str()).x + style.FramePadding.x * 2.0f; };

    float movesWidth = 0.0f;
    for (const char* token : kPreviewMoves)
        movesWidth += buttonWidth(Localized(token)) + 1.0f;

    std::string defaults = Localized("#GameUI_ViewDefaultsBtn");
    std::string presetsCaption = Localized("#GameUI_GamePresets");
    float presetsWidth = buttonWidth(defaults) + ImGui::CalcTextSize(presetsCaption.c_str()).x + style.ItemSpacing.x;
    for (const Preset& preset : presets)
        presetsWidth += buttonWidth(Localized(preset.token)) + style.ItemSpacing.x;

    // the movement buttons join into one switch
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, style.ItemSpacing.y));
    for (int i = 0; i < (int)std::size(kPreviewMoves); i++)
    {
        bool active = m_iPreviewMove == i;
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
        if (i > 0)
            ImGui::SameLine();
        if (ImGui::Button(Localized(kPreviewMoves[i]).c_str()))
            m_iPreviewMove = i;
        if (active)
            ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();

    // on one line when there's room, under the switch otherwise
    if (movesWidth + presetsWidth + style.ItemSpacing.x * 4.0f <= avail)
    {
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - movesWidth - presetsWidth - style.ItemSpacing.x);
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", presetsCaption.c_str());
    ImGui::SameLine();

    for (size_t i = 0; i < presets.size(); i++)
    {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Button(Localized(presets[i].token).c_str()))
            presets[i].apply();
        ImGui::PopID();
        ImGui::SameLine();
    }

    if (ImGui::Button(defaults.c_str()))
    {
        for (const char* cvar : kTabCvars[m_iGameTab])
        {
            if (cvar)
                ResetToDefault(cvar);
        }
    }
}

void CImGuiOptions::DrawCrosshairTab()
{
    CvarCombo("#GameUI_CrosshairType", cvars::kCrosshairType.name, {
        { "#GameUI_Crosshair_Cross", "0" },
        { "#GameUI_Crosshair_TShape", "1" },
        { "#GameUI_Crosshair_Circle", "2" },
        { "#GameUI_Crosshair_Dot", "3" },
    });

    // captions of crosshair::kSizes, in its order
    static const char* const kSizeTokens[crosshair::kSizeCount] = {
        "#GameUI_Auto", "#GameUI_Small", "#GameUI_Medium", "#GameUI_Large", "#GameUI_ExtraSmall",
    };
    std::vector<Choice> sizes;
    for (int i = 0; i < crosshair::kSizeCount; i++)
        sizes.push_back({ kSizeTokens[i], crosshair::kSizes[i].name });
    CvarCombo("#GameUI_CrosshairSize", cvars::kCrosshairSize.name, sizes);

    const char* colorCvar = cvars::kCrosshairColor.name;
    if (Exists(colorCvar))
    {
        BeginRow("#GameUI_CrosshairColor", m_Pending.count(colorCvar) != 0);
        ncl_math::Color color = ParseCrosshairColor(PendingString(colorCvar));
        float rgb[3] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f };
        if (ImGui::ColorEdit3("##CrosshairColor", rgb, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_Uint8))
        {
            char text[32];
            snprintf(text, sizeof(text), "%d %d %d", (int)std::lround(rgb[0] * 255.0f), (int)std::lround(rgb[1] * 255.0f), (int)std::lround(rgb[2] * 255.0f));
            SetPending(colorCvar, text);
        }
        EndRow();
    }

    CvarCheckbox("#GameUI_Translucent", cvars::kCrosshairTranslucent.name);
    CvarCheckbox("#GameUI_CrosshairDynamic", cvars::kDynamicCrosshair.name);
}

void CImGuiOptions::DrawBobbingTab()
{
    CvarCombo("#GameUI_BobStyle", cvars::kBobStyle.name, {
        { "#GameUI_BobStyleClassic", "0" },
        { "#GameUI_BobStyleClassicSway", "1" },
        { "#GameUI_BobStyleModern", "2" },
    });

    CvarSlider("#GameUI_BobCycle", cvars::kBobCycle.name, 0.1f, 2.0f, "%.2f");
    CvarSlider("#GameUI_BobUp", cvars::kBobUp.name, 0.05f, 0.95f, "%.2f");

    // the two styles have their own amplitudes, so only the selected style's show
    if (static_cast<int>(PendingValue(cvars::kBobStyle.name)) == view_bob::kStyleModern)
    {
        CvarSlider("#GameUI_BobAmtVert", cvars::kBobAmtVert.name, 0.0f, 0.4f, "%.2f");
        CvarSlider("#GameUI_BobAmtLat", cvars::kBobAmtLat.name, 0.0f, 0.8f, "%.2f");
        CvarSlider("#GameUI_BobLowerAmt", cvars::kBobLowerAmt.name, 0.0f, 30.0f, "%.0f");
    }
    else
        CvarSlider("#GameUI_BobAmount", cvars::kBob.name, 0.0f, 0.05f, "%.3f");
}

void CImGuiOptions::DrawModelTab()
{
    CvarSlider("#GameUI_ViewmodelOffsetX", cvars::kViewmodelOffsetX.name, -8.0f, 8.0f, "%.2f");
    CvarSlider("#GameUI_ViewmodelOffsetY", cvars::kViewmodelOffsetY.name, -8.0f, 8.0f, "%.2f");
    CvarSlider("#GameUI_ViewmodelOffsetZ", cvars::kViewmodelOffsetZ.name, -8.0f, 8.0f, "%.2f");

    // the Video page can tie it to the main FOV, which this slider would then fight
    bool followsFov = m_VideoEdited.viewmodelFovAuto;
    if (followsFov)
        SetNextRowHint("#GameUI_OptionsViewmodelFovFollows");
    ImGui::BeginDisabled(followsFov);
    CvarSlider("#GameUI_ViewmodelFov", cvars::kViewmodelFov.name, 70.0f, 100.0f, "%.0f");
    ImGui::EndDisabled();

    CvarCheckbox("#GameUI_ViewmodelDisableShift", cvars::kViewmodelDisableShift.name);
}

void CImGuiOptions::DrawInertiaTab()
{
    CvarCombo("#GameUI_ViewLagStyle", cvars::kViewmodelLagStyle.name, {
        { "#GameUI_ViewLagOff", "0" },
        { "#GameUI_ViewLagHL2", "1" },
        { "#GameUI_ViewLagCSS", "2" },
    });

    // the scale works for both lag styles, the speed only for HL2's
    int style = static_cast<int>(PendingValue(cvars::kViewmodelLagStyle.name));
    ImGui::BeginDisabled(style == 0);
    CvarSlider("#GameUI_ViewLagScale", cvars::kViewmodelLagScale.name, 0.0f, 5.0f, "%.2f");
    ImGui::EndDisabled();
    ImGui::BeginDisabled(style != 1);
    CvarSlider("#GameUI_ViewLagSpeed", cvars::kViewmodelLagSpeed.name, 1.0f, 20.0f, "%.1f");
    ImGui::EndDisabled();
}

void CImGuiOptions::DrawCameraTab()
{
    CvarSlider("#GameUI_RollAngle", cvars::kRollAngle.name, 0.0f, 10.0f, "%.1f");
    CvarSlider("#GameUI_RollSpeed", cvars::kRollSpeed.name, 10.0f, 400.0f, "%.0f");
    CvarSlider("#GameUI_CameraMoveScale", cvars::kCameraMovementScale.name, 0.0f, 2.0f, "%.2f");
    CvarSlider("#GameUI_CameraMoveInterp", cvars::kCameraMovementInterp.name, 0.0f, 0.5f, "%.2f");

    // the modern bob style never moves the camera
    ImGui::BeginDisabled(static_cast<int>(PendingValue(cvars::kBobStyle.name)) == view_bob::kStyleModern);
    SetNextRowHint("#GameUI_BobCameraTooltip");
    CvarCheckbox("#GameUI_BobCamera", cvars::kBobCamera.name);
    ImGui::EndDisabled();
}

// the old Keyboard page's, from a VGUI key code to the engine's
int ConvertVGUIToEngine(vgui2::KeyCode code);

namespace
{
    // the engine's tokenizer, which kb_act.lst and kb_def.lst are written for: quoted strings and
    // // comments; empty once the text runs out
    std::vector<std::string> ParseTokens(const char* path)
    {
        std::vector<std::string> tokens;
        std::string text = ReadFile(path);
        char token[1024];
        for (char* data = text.data(); data;)
        {
            data = engine->COM_ParseFile(data, token);
            if (!token[0])
                break;
            tokens.push_back(token);
        }
        return tokens;
    }

    // takes the key off every action: an action losing its key moves its alternate up
    void RemoveKey(std::vector<CImGuiOptions::Binding>& bindings, const std::string& key)
    {
        for (auto& binding : bindings)
        {
            if (!V_stricmp(binding.altKey.c_str(), key.c_str()))
                binding.altKey.clear();

            if (!V_stricmp(binding.key.c_str(), key.c_str()))
            {
                binding.key = binding.altKey;
                binding.altKey.clear();
            }
        }
    }

    // as the old page did: a new key goes first and the old one becomes the alternate
    void AddKey(std::vector<CImGuiOptions::Binding>& bindings, size_t row, const std::string& key, int slot)
    {
        CImGuiOptions::Binding& binding = bindings[row];
        if (!V_stricmp((slot == 0 ? binding.key : binding.altKey).c_str(), key.c_str()))
            return;

        RemoveKey(bindings, key);

        if (slot == 0)
        {
            binding.altKey = binding.key;
            binding.key = key;
        }
        else if (binding.key.empty())
            binding.key = key;
        else
            binding.altKey = key;
    }

    CImGuiOptions::Binding* FindBinding(std::vector<CImGuiOptions::Binding>& bindings, const char* command)
    {
        for (auto& binding : bindings)
        {
            if (!binding.header && !V_stricmp(binding.command.c_str(), command))
                return &binding;
        }
        return nullptr;
    }

    // the engine names keys in lower case, which reads better as the keyboard prints them
    std::string KeyDisplayName(const std::string& key)
    {
        std::string text = key;
        for (char& c : text)
            c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        return text;
    }
}

void CImGuiOptions::LoadBindings()
{
    m_Bindings.clear();
    m_KeysToUnbind.clear();
    m_iCaptureRow = -1;

    // pairs of a command and its description; "blank" starts a section, "=====" lines are decoration
    std::vector<std::string> tokens = ParseTokens("gfx/shell/kb_act.lst");
    for (size_t i = 0; i + 1 < tokens.size(); i += 2)
    {
        if (tokens[i + 1][0] == '=')
            continue;

        Binding binding;
        binding.command = tokens[i];
        binding.description = tokens[i + 1];
        binding.header = !V_stricmp(tokens[i].c_str(), "blank");
        m_Bindings.push_back(binding);
    }

    for (int key = 0; key < 256; key++)
    {
        const char* command = g_pGameUIFuncs->Key_BindingForKey(key);
        const char* name = g_pGameUIFuncs->Key_NameForKey(key);
        if (!command || !command[0] || !name || !name[0])
            continue;

        if (Binding* binding = FindBinding(m_Bindings, command))
        {
            AddKey(m_Bindings, binding - m_Bindings.data(), name, 0);
            m_KeysToUnbind.push_back(name);
        }
    }

    m_BindingsSaved = m_Bindings;
}

void CImGuiOptions::LoadDefaultBindings()
{
    for (auto& binding : m_Bindings)
        binding.key.clear(), binding.altKey.clear();

    // pairs of a key and a command
    std::vector<std::string> tokens = ParseTokens("gfx/shell/kb_def.lst");
    for (size_t i = 0; i + 1 < tokens.size(); i += 2)
    {
        if (Binding* binding = FindBinding(m_Bindings, tokens[i + 1].c_str()))
            AddKey(m_Bindings, binding - m_Bindings.data(), tokens[i], 0);
    }

    // whatever the file says, the console and the menu stay reachable
    if (Binding* binding = FindBinding(m_Bindings, "toggleconsole"))
        AddKey(m_Bindings, binding - m_Bindings.data(), "`", 0);
    if (Binding* binding = FindBinding(m_Bindings, "cancelselect"))
        AddKey(m_Bindings, binding - m_Bindings.data(), "ESCAPE", 0);
}

void CImGuiOptions::SaveBindings()
{
    bool changed = false;
    for (size_t i = 0; i < m_Bindings.size() && i < m_BindingsSaved.size(); i++)
        changed |= m_Bindings[i].key != m_BindingsSaved[i].key || m_Bindings[i].altKey != m_BindingsSaved[i].altKey;
    if (!changed)
        return;

    char command[512];
    for (const std::string& key : m_KeysToUnbind)
    {
        snprintf(command, sizeof(command), "unbind \"%s\"\n", key.c_str());
        engine->pfnClientCmd(command);
    }

    m_KeysToUnbind.clear();
    for (const Binding& binding : m_Bindings)
    {
        for (const std::string* key : { &binding.key, &binding.altKey })
        {
            if (binding.header || key->empty())
                continue;

            snprintf(command, sizeof(command), "bind \"%s\" \"%s\"\n", key->c_str(), binding.command.c_str());
            engine->pfnClientCmd(command);
            m_KeysToUnbind.push_back(*key);
        }
    }

    // the player's own binds come back on top, as with the old page
    engine->pfnClientCmd("exec userconfig.cfg\n");

    // the engine runs the commands later, so the list can't be read back from it yet
    m_BindingsSaved = m_Bindings;
}

void CImGuiOptions::FinishCapture(const char* keyName)
{
    if (m_iCaptureRow >= 0 && m_iCaptureRow < (int)m_Bindings.size() && keyName && keyName[0])
        AddKey(m_Bindings, m_iCaptureRow, keyName, m_iCaptureSlot);

    m_iCaptureRow = -1;
}

void CImGuiOptions::OnKeyCodePressed(vgui2::KeyCode code)
{
    if (m_iCaptureRow < 0)
    {
        BaseClass::OnKeyCodePressed(code);
        return;
    }

    // Escape only leaves the capture, so the menu key can't be bound away by accident
    if (code == vgui2::KEY_ESCAPE)
    {
        m_iCaptureRow = -1;
        return;
    }

    int key = ConvertVGUIToEngine(code);
    if (key > 0)
        FinishCapture(g_pGameUIFuncs->Key_NameForKey(key));
}

void CImGuiOptions::OnKeyCodeTyped(vgui2::KeyCode code)
{
    if (m_iCaptureRow < 0)
        BaseClass::OnKeyCodeTyped(code);
}

void CImGuiOptions::OnMousePressed(vgui2::MouseCode code)
{
    if (m_iCaptureRow < 0)
    {
        BaseClass::OnMousePressed(code);
        return;
    }

    switch (code)
    {
        case vgui2::MOUSE_RIGHT: FinishCapture("MOUSE2"); break;
        case vgui2::MOUSE_MIDDLE: FinishCapture("MOUSE3"); break;
        case vgui2::MOUSE_4: FinishCapture("MOUSE4"); break;
        case vgui2::MOUSE_5: FinishCapture("MOUSE5"); break;
        default: FinishCapture("MOUSE1"); break;
    }
}

void CImGuiOptions::OnMouseDoublePressed(vgui2::MouseCode code)
{
    if (m_iCaptureRow < 0)
        BaseClass::OnMouseDoublePressed(code);
    else
        OnMousePressed(code);
}

void CImGuiOptions::OnMouseWheeled(int delta)
{
    if (m_iCaptureRow < 0)
        BaseClass::OnMouseWheeled(delta);
    else
        FinishCapture(delta > 0 ? "MWHEELUP" : "MWHEELDOWN");
}

void CImGuiOptions::DrawKeyboard()
{
    const ImGuiStyle& style = ImGui::GetStyle();
    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);

    std::string defaults = Localized("#GameUI_UseDefaults", "Use defaults");
    float defaultsWidth = ImGui::CalcTextSize(defaults.c_str()).x + style.FramePadding.x * 2.0f;
    ImGui::SetNextItemWidth(std::min(320.0f, ImGui::GetContentRegionAvail().x - defaultsWidth - style.ItemSpacing.x));
    ImGui::InputTextWithHint("##BindingSearch", Localized("#GameUI_OptionsBindingSearch", "Find an action or a key").c_str(), m_szBindingSearch, sizeof(m_szBindingSearch));
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - defaultsWidth);
    if (ImGui::Button(defaults.c_str()))
        m_bOpenDefaultsPopup = true;

    // the old page asked too: every binding goes at once
    if (m_bOpenDefaultsPopup)
    {
        ImGui::OpenPopup("##DefaultBindings");
        m_bOpenDefaultsPopup = false;
    }
    if (ImGui::BeginPopupModal("##DefaultBindings", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::TextColored(accent, "%s", Localized("#GameUI_KeyboardSettings").c_str());
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
        ImGui::TextUnformatted(Localized("#GameUI_KeyboardSettingsText").c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        if (ImGui::Button(Localized("#GameUI_OK").c_str(), ImVec2(120, 0)))
        {
            LoadDefaultBindings();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(Localized("#GameUI_Cancel").c_str(), ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::Dummy(ImVec2(0, 2));

    // the search matches the action's text or either key, in any case
    std::string search = m_szBindingSearch;
    for (char& c : search)
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    auto matches = [&](const Binding& binding, const std::string& description)
    {
        if (search.empty())
            return true;
        for (std::string text : { description, binding.key, binding.altKey })
        {
            for (char& c : text)
                c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            if (text.find(search) != std::string::npos)
                return true;
        }
        return false;
    };

    // each section of kb_act.lst is a card, each action a row with its two keys as key caps
    bool cardOpen = false;
    for (int row = 0; row < (int)m_Bindings.size(); row++)
    {
        Binding& binding = m_Bindings[row];
        std::string description = !binding.description.empty() && binding.description[0] == '#' ? Localized(binding.description.c_str(), binding.description.c_str() + 1) : binding.description;

        if (binding.header)
        {
            // a section none of whose actions the search found stays closed
            bool any = false;
            for (int next = row + 1; next < (int)m_Bindings.size() && !m_Bindings[next].header; next++)
            {
                const Binding& item = m_Bindings[next];
                std::string text = !item.description.empty() && item.description[0] == '#' ? Localized(item.description.c_str(), item.description.c_str() + 1) : item.description;
                any |= matches(item, text);
            }

            if (cardOpen)
                EndCard();
            cardOpen = any;
            if (any)
                BeginCard(binding.description.c_str(), description.c_str());
            continue;
        }

        if (!cardOpen || !matches(binding, description))
            continue;

        ImGui::PushID(row);
        bool pending = row < (int)m_BindingsSaved.size()
            && (binding.key != m_BindingsSaved[row].key || binding.altKey != m_BindingsSaved[row].altKey);
        BeginRowText(description, pending);

        float capsWidth = ImGui::CalcItemWidth();
        float capWidth = (capsWidth - style.ItemSpacing.x) * 0.5f;
        ImVec2 capsPos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        for (int slot = 0; slot < 2; slot++)
        {
            const std::string& key = slot == 0 ? binding.key : binding.altKey;
            bool capturing = m_iCaptureRow == row && m_iCaptureSlot == slot;

            ImVec2 min(capsPos.x + slot * (capWidth + style.ItemSpacing.x), capsPos.y);
            ImVec2 max(min.x + capWidth, min.y + ImGui::GetFrameHeight());
            ImGui::SetCursorScreenPos(min);
            ImGui::PushID(slot);
            if (ImGui::InvisibleButton("##Key", ImVec2(capWidth, ImGui::GetFrameHeight()), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight))
            {
                // right-click takes the key away, which leaves it bound to nothing
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
                {
                    if (!key.empty())
                        RemoveKey(m_Bindings, std::string(key));
                }
                else
                {
                    m_iCaptureRow = row;
                    m_iCaptureSlot = slot;
                }
            }
            bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();

            // a key cap: a raised face with a darker lip under it
            ImU32 face = ImGui::GetColorU32(hovered || capturing ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
            drawList->AddRectFilled(ImVec2(min.x, min.y + 2.0f), ImVec2(max.x, max.y + 2.0f), ImGui::GetColorU32(ImGuiCol_Border), 4.0f);
            drawList->AddRectFilled(min, max, face, 4.0f);
            drawList->AddRect(min, max, capturing ? ImGui::GetColorU32(accent) : ImGui::GetColorU32(ImGuiCol_Border), 4.0f, 0, capturing ? 2.0f : 1.0f);

            std::string text;
            ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
            if (capturing)
            {
                // blinks, so the cap that waits is easy to find
                text = Localized("#GameUI_OptionsPressKey", "Press a key");
                float blink = 0.55f + 0.45f * std::sin(static_cast<float>(ImGui::GetTime()) * 6.0f);
                color = ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, blink));
            }
            else if (key.empty())
            {
                text = "-";
                color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
            }
            else
                text = KeyDisplayName(key);

            ImVec2 size = ImGui::CalcTextSize(text.c_str());
            ImGui::PushClipRect(min, max, true);
            drawList->AddText(ImVec2(min.x + std::max(4.0f, (capWidth - size.x) * 0.5f), min.y + (max.y - min.y - size.y) * 0.5f), color, text.c_str());
            ImGui::PopClipRect();
        }

        ImGui::SetCursorScreenPos(ImVec2(capsPos.x, capsPos.y));
        ImGui::Dummy(ImVec2(capsWidth, ImGui::GetFrameHeight() + 2.0f));
        EndRow();
        ImGui::PopID();
    }

    if (cardOpen)
        EndCard();
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

bool CImGuiOptions::CvarCheckbox(const char* token, const char* cvar)
{
    if (!Exists(cvar))
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
    if (!Exists(cvar))
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

bool CImGuiOptions::CvarText(const char* token, const char* cvar, bool password)
{
    if (!Exists(cvar))
    {
        m_RowHint.clear();
        return false;
    }

    BeginRow(token, m_Pending.count(cvar) != 0);

    char text[128];
    V_strncpy(text, PendingString(cvar).c_str(), sizeof(text));
    ImGui::PushID(cvar);
    bool changed = ImGui::InputText("##Value", text, sizeof(text), password ? ImGuiInputTextFlags_Password : 0);
    ImGui::PopID();
    EndRow();

    // quotes would end the command that sets the value early
    if (changed)
    {
        UTIL_StripInvalidCharacters(text, sizeof(text));
        SetPending(cvar, text);
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
    if (!Exists(cvar))
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
    if (!Exists(cvar))
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

std::string CImGuiOptions::CurrentString(const char* name) const
{
    if (m_SetInfoKeys.count(name))
    {
        const char* value = engine->LocalPlayerInfo_ValueForKey(name);
        return value ? value : "";
    }

    cvar_t* pointer = engine->pfnGetCvarPointer(name);
    return pointer ? pointer->string : "";
}

bool CImGuiOptions::Exists(const char* name) const
{
    return m_SetInfoKeys.count(name) || engine->pfnGetCvarPointer(name);
}

std::string CImGuiOptions::PendingString(const char* cvar) const
{
    auto it = m_Pending.find(cvar);
    if (it != m_Pending.end())
        return it->second;

    return CurrentString(cvar);
}

float CImGuiOptions::PendingValue(const char* cvar) const
{
    return static_cast<float>(atof(PendingString(cvar).c_str()));
}

void CImGuiOptions::SetPending(const char* cvar, const std::string& value)
{
    // a control moved back to where it was leaves nothing to apply
    if (Exists(cvar) && SameValue(CurrentString(cvar).c_str(), value.c_str()))
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

    size_t bindings = 0;
    for (size_t i = 0; i < m_Bindings.size() && i < m_BindingsSaved.size(); i++)
        bindings += m_Bindings[i].key != m_BindingsSaved[i].key || m_Bindings[i].altKey != m_BindingsSaved[i].altKey;

    return m_Pending.size() + m_PendingKeys.size() + misc + voice + video + bindings;
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
    bool sprayChanged = m_Pending.count("cl_logofile") || m_Pending.count("cl_logocolor");
    SaveAdvancedOptions();

    for (const auto& [cvar, value] : m_Pending)
    {
        // userinfo keys and the options of user.scr that aren't the client's cvars go as commands
        char command[512];
        if (m_SetInfoKeys.count(cvar))
            snprintf(command, sizeof(command), "setinfo %s \"%s\"\n", cvar.c_str(), value.c_str());
        else if (!engine->pfnGetCvarPointer(cvar.c_str()))
            snprintf(command, sizeof(command), "%s \"%s\"\n", cvar.c_str(), value.c_str());
        else
        {
            engine->Cvar_Set(cvar.c_str(), value.c_str());
            continue;
        }
        engine->pfnClientCmd(command);
    }

    if (sprayChanged)
        SaveSpray();

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
    SaveBindings();
    // last, since it may restart the game
    SaveVideoSettings(restartForCvars);
}

void CImGuiOptions::LoadMultiplayerSettings()
{
    m_Logos.clear();
    FileFindHandle_t handle = 0;
    for (const char* file = g_pFullFileSystem->FindFirst("logos/*.bmp", &handle); file; file = g_pFullFileSystem->FindNext(handle))
    {
        // remapped.bmp is the old dialog's preview, not a spray
        std::string name = file;
        if (name.size() > 4 && V_stricmp(file, "remapped.bmp") && name[0] != '.')
            m_Logos.push_back(name.substr(0, name.size() - 4));
    }
    g_pFullFileSystem->FindClose(handle);
    std::sort(m_Logos.begin(), m_Logos.end());

    m_pAdvancedOptions = std::make_unique<CInfoDescription>(nullptr);
    m_pAdvancedOptions->InitFromFile("user.scr");

    // the old dialog adds this one when the game's script lacks it
    if (!m_pAdvancedOptions->FindObject("hud_deathnotice_old"))
    {
        auto* option = new CScriptObject();
        option->type = O_BOOL;
        V_strcpy_safe(option->prompt, "#Cstrike_LegacyKillFeed");
        V_strcpy_safe(option->cvarname, "hud_deathnotice_old");
        m_pAdvancedOptions->AddObject(option);
    }

    m_pAdvancedOptions->TransferCurrentValues(nullptr);

    for (CScriptObject* option = m_pAdvancedOptions->pObjList; option; option = option->pNext)
    {
        if (option->bSetInfo)
            m_SetInfoKeys.insert(option->cvarname);
    }
}

void CImGuiOptions::SaveAdvancedOptions()
{
    if (!m_pAdvancedOptions)
        return;

    bool changed = false;
    for (CScriptObject* option = m_pAdvancedOptions->pObjList; option; option = option->pNext)
    {
        auto it = m_Pending.find(option->cvarname);
        if (it == m_Pending.end())
            continue;

        option->SetCurValue(it->second.c_str());
        changed = true;
    }

    if (!changed)
        return;

    FileHandle_t file = g_pFullFileSystem->Open("user.scr", "wb");
    if (file != FILESYSTEM_INVALID_HANDLE)
    {
        m_pAdvancedOptions->WriteToScriptFile(file);
        g_pFullFileSystem->Close(file);
    }
}

void CImGuiOptions::SaveSpray()
{
    const SprayColor& color = SprayColorByName(CurrentString("cl_logocolor"));
    int width, height;
    std::string dib = LoadSprayDib(CurrentString("cl_logofile"), color, width, height);
    if (dib.empty())
        return;

    // UpdateLogoWAD locks what it's given like a Windows memory handle
    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, dib.size());
    memcpy(GlobalLock(handle), dib.data(), dib.size());
    GlobalUnlock(handle);
    UpdateLogoWAD(handle, color.r, color.g, color.b);
    GlobalFree(handle);
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
