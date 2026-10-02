#include "GameUi.h"
#include "ImGuiConsole.h"
#include "GameConsole.h"
#include "IBaseUI.h"
#include "IGameUIFuncs.h"
#include "LoadingDialog.h"
#include <FileSystem.h>
#include <tier1/strtools.h>
#include <vgui/ILocalize.h>
#include <cvardef.h>

#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Controls.h>

#include <console_buffer/console_buffer.h>
#include <console_buffer/completion.h>
#include <console_buffer/kinds.h>
#include <console_buffer/log_file.h>
#include <console_buffer/selection.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <cfloat>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui2;

static const char* const kHistoryFile = "console_history.txt";
static const size_t kMaxSuggestions = 10;

static float CvarValue(const char* name, float fallback)
{
    cvar_t* cvar = engine->pfnGetCvarPointer(name);
    return cvar ? cvar->value : fallback;
}

static bool CvarOn(const char* name, bool fallback)
{
    return CvarValue(name, fallback ? 1.0f : 0.0f) != 0.0f;
}

CImGuiConsole::CImGuiConsole(console_buffer::ConsoleBuffer& scrollback) : BaseClass("console_layout.ini"), m_Scrollback(scrollback)
{
    SetVisible(false);
    LoadHistory();
}

void CImGuiConsole::LoadHistory()
{
    FileHandle_t file = g_pFullFileSystem->Open(kHistoryFile, "rb");
    if (file == FILESYSTEM_INVALID_HANDLE)
        return;

    std::string text(g_pFullFileSystem->Size(file), '\0');
    g_pFullFileSystem->Read(text.data(), static_cast<int>(text.size()), file);
    g_pFullFileSystem->Close(file);

    m_History.Load(text);
}

void CImGuiConsole::SaveHistory()
{
    FileHandle_t file = g_pFullFileSystem->Open(kHistoryFile, "wb");
    if (file == FILESYSTEM_INVALID_HANDLE)
        return;

    std::string text = m_History.Save();
    g_pFullFileSystem->Write(text.data(), static_cast<int>(text.size()), file);
    g_pFullFileSystem->Close(file);
}

static void ReplaceInput(ImGuiInputTextCallbackData* data, const std::string& text)
{
    data->DeleteChars(0, data->BufTextLen);
    data->InsertChars(0, text.c_str());
}

int CImGuiConsole::OnInputCallback(ImGuiInputTextCallbackData* data)
{
    auto* console = static_cast<CImGuiConsole*>(data->UserData);
    bool up = data->EventKey == ImGuiKey_UpArrow;

    switch (data->EventFlag)
    {
        case ImGuiInputTextFlags_CallbackAlways:
            if (console->m_bCursorToEnd)
            {
                data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen;
                data->InsertChars(data->CursorPos, console->m_PendingInput.c_str());
                console->m_PendingInput.clear();
                console->m_bCursorToEnd = false;
            }
            break;

        case ImGuiInputTextFlags_CallbackCompletion:
            if (!console->m_Suggestions.empty())
                ReplaceInput(data, console->m_Suggestions[console->m_iSuggestion] + " ");
            break;

        case ImGuiInputTextFlags_CallbackHistory:
            // Up and Down pick a suggestion while there are any, and walk the history otherwise
            if (!console->m_Suggestions.empty())
            {
                int last = static_cast<int>(console->m_Suggestions.size()) - 1;
                console->m_iSuggestion = std::clamp(console->m_iSuggestion + (up ? -1 : 1), 0, last);
            }
            else if (std::optional<std::string> entry = up ? console->m_History.Older() : console->m_History.Newer())
            {
                ReplaceInput(data, *entry);
                console->m_RecalledText = *entry;
            }
            break;
    }

    return 0;
}

void CImGuiConsole::RebuildCompletionNames()
{
    m_CompletionNames.clear();

    for (auto cmd = engine->GetFirstCmdFunctionHandle(); cmd; cmd = engine->GetNextCmdFunctionHandle(cmd))
        m_CompletionNames.emplace_back(engine->GetCmdFunctionName(cmd));

    for (cvar_t* cvar = engine->GetFirstCvarPtr(); cvar; cvar = cvar->next)
        m_CompletionNames.emplace_back(cvar->name);

    std::sort(m_CompletionNames.begin(), m_CompletionNames.end());
    m_CompletionNames.erase(std::unique(m_CompletionNames.begin(), m_CompletionNames.end()), m_CompletionNames.end());
}

// a token of resource/console_<language>.txt in UTF-8, "" when it's missing
std::string CImGuiConsole::Localized(const char* token)
{
    std::string utf8;
    if (const wchar_t* wide = g_pVGuiLocalize->Find(token))
    {
        utf8.resize(wcslen(wide) * 4 + 1);
        V_UnicodeToUTF8(wide, utf8.data(), static_cast<int>(utf8.size()));
        utf8.resize(strlen(utf8.c_str()));
    }

    return utf8;
}

// the description of a command or cvar, "" for the ones the help doesn't cover
const std::string& CImGuiConsole::Describe(const std::string& name)
{
    auto found = m_Descriptions.find(name);
    if (found != m_Descriptions.end())
        return found->second;

    std::string token = "#Console_Help_" + name;
    return m_Descriptions.emplace(name, Localized(token.c_str())).first->second;
}

void CImGuiConsole::UpdateSuggestions()
{
    if (!CvarOn("con_suggestions", true))
    {
        m_Suggestions.clear();
        return;
    }

    std::string_view typed = m_szInput;
    if (typed.empty())
        m_RecalledText.clear();

    // suggest while the first word is being typed, not over its arguments
    bool typingName = !typed.empty() && typed.find(' ') == std::string_view::npos && typed != m_RecalledText;
    if (!typingName)
    {
        m_Suggestions.clear();
        m_SuggestionsFor.clear();
        return;
    }

    if (typed == m_SuggestionsFor)
        return;

    m_SuggestionsFor = typed;
    m_Suggestions = console_buffer::MatchNames(m_CompletionNames, typed, kMaxSuggestions);
    m_iSuggestion = 0;
}

void CImGuiConsole::AcceptSuggestion(const std::string& name)
{
    V_snprintf(m_szInput, sizeof(m_szInput), "%s ", name.c_str());
    m_Suggestions.clear();
    m_bFocusInput = true;
}

void CImGuiConsole::DrawSuggestions()
{
    if (!CvarOn("con_suggestions", true))
        return;

    // the matches while a name is typed, or what the typed command is once it has a space after it
    std::vector<std::string> rows = m_Suggestions;
    bool picking = !rows.empty();
    if (!picking)
    {
        std::string_view typed = m_szInput;
        size_t space = typed.find(' ');
        if (space == std::string_view::npos || space == 0)
            return;

        std::string name(typed.substr(0, space));
        if (!std::binary_search(m_CompletionNames.begin(), m_CompletionNames.end(), name))
            return;

        rows.push_back(name);
    }

    // above the input line, as wide as the console at most
    ImGui::SetNextWindowPos(ImVec2(m_flInputX, m_flInputTop - ImGui::GetStyle().ItemSpacing.y), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(m_flConsoleWidth, FLT_MAX));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_AlwaysAutoResize;

    if (ImGui::Begin("##Suggestions", nullptr, flags))
    {
        // clicking the console would otherwise put it over the list
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());

        if (ImGui::BeginTable("##Rows", 3, ImGuiTableFlags_SizingFixedFit))
        {
            for (int i = 0; i < static_cast<int>(rows.size()); i++)
            {
                const std::string& name = rows[i];
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                if (picking)
                {
                    if (ImGui::Selectable(name.c_str(), i == m_iSuggestion, ImGuiSelectableFlags_SpanAllColumns))
                        AcceptSuggestion(name);
                }
                else
                {
                    ImGui::TextUnformatted(name.c_str());
                }

                ImGui::TableNextColumn();
                if (cvar_t* cvar = engine->pfnGetCvarPointer(name.c_str()))
                    ImGui::TextColored(ImGui::GetStyle().Colors[ImGuiCol_CheckMark], "%s", cvar->string);

                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", Describe(name).c_str());
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
}

void CImGuiConsole::Activate()
{
    SetVisible(true);
    MoveToFront();
    RequestFocus();

    // config.cfg has run by the first time the console opens, so the settings are the player's
    if (!m_bOpenedOnce)
    {
        m_bOpenedOnce = true;
        GameConsole().RestorePreviousSession();
        LoadFilters();
    }

    ResetInput();
    RebuildCompletionNames();
    m_bFocusWindow = true;
    m_bFocusInput = true;
    m_bIgnoreNextChar = false;
}

void CImGuiConsole::DrawImGui()
{
    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(720, 420), ImGuiCond_FirstUseEver);

    // the input line only takes the keyboard focus inside a focused window
    if (m_bFocusWindow)
    {
        ImGui::SetNextWindowFocus();
        m_bFocusWindow = false;
    }

    ApplySettings();
    float opacity = std::clamp(CvarValue("con_opacity", 1.0f), 0.3f, 1.0f);
    ImGui::SetNextWindowBgAlpha(opacity);

    bool open = true;
    bool expanded = ImGui::Begin("Console", &open, ImGuiWindowFlags_NoCollapse);

    if (expanded)
    {
        DrawToolbar();

        // leave one row under the scrollback for the input line
        float footer = ImGui::GetFrameHeightWithSpacing();
        ImVec4 childBg = ImGui::GetStyle().Colors[ImGuiCol_ChildBg];
        childBg.w *= opacity;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, childBg);
        ImGui::BeginChild("Scrollback", ImVec2(0, -footer));
        DrawScrollback();
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // Typing after selecting something in the scrollback goes back to the input line.
        // The focus comes with the whole input selected, so the typed letters wait aside
        // and go to its end once it's active, instead of replacing everything typed.
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput && !io.InputQueueCharacters.empty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
        {
            for (ImWchar ch : io.InputQueueCharacters)
            {
                char utf8[5];
                ImTextCharToUtf8(utf8, ch);
                m_PendingInput += utf8;
            }
            io.InputQueueCharacters.resize(0);
            m_bFocusInput = true;
        }

        if (m_bFocusInput)
        {
            ImGui::SetKeyboardFocusHere();
            m_bFocusInput = false;
            m_bCursorToEnd = true;
        }

        const char* submitLabel = "Submit";
        float submitWidth = ImGui::CalcTextSize(submitLabel).x + ImGui::GetStyle().FramePadding.x * 2;

        ImGui::SetNextItemWidth(-(submitWidth + ImGui::GetStyle().ItemSpacing.x));
        bool submitted = ImGui::InputText("##Input", m_szInput, sizeof(m_szInput),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion |
            ImGuiInputTextFlags_CallbackAlways, OnInputCallback, this);
        m_flInputX = ImGui::GetItemRectMin().x;
        m_flInputTop = ImGui::GetItemRectMin().y;
        m_flConsoleWidth = ImGui::GetWindowWidth();
        ImGui::SameLine();
        submitted |= ImGui::Button(submitLabel, ImVec2(submitWidth, 0));

        if (submitted)
        {
            if (m_szInput[0] != '\0')
                Execute(m_szInput);

            m_szInput[0] = '\0';
            // Enter takes the focus off the input line
            m_bFocusInput = true;
        }
    }

    ImGui::End();

    if (expanded)
    {
        UpdateSuggestions();
        DrawSuggestions();
    }

    if (!open)
        SetVisible(false);

    // the old console takes over only once this frame is done with the new one
    if (m_bSwitchToLegacy)
    {
        m_bSwitchToLegacy = false;
        SetVisible(false);
        GameConsole().Activate();
    }
}

void CImGuiConsole::ApplySettings()
{
    SetFontSize(std::clamp(std::round(CvarValue("con_fontsize", 16.0f)), 10.0f, 28.0f));

    size_t maxLines = static_cast<size_t>(std::clamp(CvarValue("con_maxlines", 5000.0f), 100.0f, 20000.0f));
    if (maxLines != m_iMaxLines)
    {
        m_iMaxLines = maxLines;
        m_Scrollback.SetMaxLines(maxLines);
    }
}

// con_filters holds the hidden topics as a bit mask and whether only problems show: "12 1"
void CImGuiConsole::LoadFilters()
{
    cvar_t* filters = engine->pfnGetCvarPointer("con_filters");
    if (!CvarOn("con_keepfilters", true) || !filters || !filters->string)
        return;

    unsigned int hidden = 0;
    int problemsOnly = 0;
    if (sscanf(filters->string, "%u %d", &hidden, &problemsOnly) != 2)
        return;

    for (int topic = 0; topic < console_buffer::kTopicCount; topic++)
        m_bTopicVisible[topic] = !(hidden & (1u << topic));
    m_bProblemsOnly = problemsOnly != 0;
}

void CImGuiConsole::SaveFilters()
{
    if (!CvarOn("con_keepfilters", true))
        return;

    unsigned int hidden = 0;
    for (int topic = 0; topic < console_buffer::kTopicCount; topic++)
    {
        if (!m_bTopicVisible[topic])
            hidden |= 1u << topic;
    }

    char value[32];
    V_snprintf(value, sizeof(value), "%u %d", hidden, m_bProblemsOnly ? 1 : 0);
    engine->Cvar_Set("con_filters", value);
}

static void SettingCheckbox(const std::string& label, const char* cvar, bool fallback)
{
    bool on = CvarOn(cvar, fallback);
    if (ImGui::Checkbox(label.c_str(), &on))
        engine->Cvar_SetValue(cvar, on ? 1.0f : 0.0f);
}

void CImGuiConsole::DrawSettings()
{
    if (!ImGui::BeginPopup("##Settings"))
        return;

    ImGui::TextDisabled("%s", Localized("#Console_Settings_Title").c_str());
    ImGui::Separator();

    SettingCheckbox(Localized("#Console_Settings_Timestamps"), "con_timestamps", false);
    SettingCheckbox(Localized("#Console_Settings_Collapse"), "con_collapse", true);
    SettingCheckbox(Localized("#Console_Settings_Suggestions"), "con_suggestions", true);
    ImGui::Separator();

    SettingCheckbox(Localized("#Console_Settings_Log"), "con_log", true);
    SettingCheckbox(Localized("#Console_Settings_Restore"), "con_restore", true);
    SettingCheckbox(Localized("#Console_Settings_KeepFilters"), "con_keepfilters", true);
    ImGui::Separator();

    ImGui::SetNextItemWidth(160.0f);
    int fontSize = static_cast<int>(std::round(CvarValue("con_fontsize", 16.0f)));
    if (ImGui::SliderInt(Localized("#Console_Settings_FontSize").c_str(), &fontSize, 12, 22))
        engine->Cvar_SetValue("con_fontsize", static_cast<float>(fontSize));

    ImGui::SetNextItemWidth(160.0f);
    int opacity = static_cast<int>(std::round(CvarValue("con_opacity", 1.0f) * 100.0f));
    if (ImGui::SliderInt(Localized("#Console_Settings_Opacity").c_str(), &opacity, 30, 100, "%d%%"))
        engine->Cvar_SetValue("con_opacity", opacity / 100.0f);

    static const int kLineLimits[] = { 1000, 5000, 20000 };
    int maxLines = static_cast<int>(CvarValue("con_maxlines", 5000.0f));
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo(Localized("#Console_Settings_MaxLines").c_str(), std::to_string(maxLines).c_str()))
    {
        for (int limit : kLineLimits)
        {
            if (ImGui::Selectable(std::to_string(limit).c_str(), limit == maxLines))
                engine->Cvar_SetValue("con_maxlines", static_cast<float>(limit));
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();

    bool legacy = false;
    if (ImGui::Checkbox(Localized("#Console_Settings_Legacy").c_str(), &legacy))
    {
        engine->Cvar_SetValue("con_legacy", 1.0f);
        m_bSwitchToLegacy = true;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

struct TopicStyle
{
    console_buffer::Topic topic;
    const char* label;
    // 0 means the theme's text color
    ImU32 color;
    // a bar on the left and a tint over the row, for the topics worth noticing
    bool marked;
};

// in the order the filter buttons show them
static const TopicStyle kTopicStyles[] = {
    { console_buffer::Topic::Chat, "#Console_Filter_Chat", IM_COL32(110, 180, 230, 255), true },
    { console_buffer::Topic::Players, "#Console_Filter_Players", IM_COL32(140, 205, 120, 255), false },
    { console_buffer::Topic::Server, "#Console_Filter_Server", IM_COL32(200, 190, 140, 255), false },
    { console_buffer::Topic::Connection, "#Console_Filter_Connection", IM_COL32(110, 200, 190, 255), false },
    { console_buffer::Topic::Commands, "#Console_Filter_Commands", IM_COL32(214, 205, 110, 255), false },
    { console_buffer::Topic::System, "#Console_Filter_System", 0, false },
    { console_buffer::Topic::Developer, "#Console_Filter_Developer", IM_COL32(150, 160, 140, 255), false },
};

static const ImU32 kErrorColor = IM_COL32(240, 110, 95, 255);
static const ImU32 kWarningColor = IM_COL32(232, 185, 74, 255);

static const TopicStyle& StyleOf(console_buffer::Topic topic)
{
    for (const TopicStyle& style : kTopicStyles)
    {
        if (style.topic == topic)
            return style;
    }

    return kTopicStyles[std::size(kTopicStyles) - 1];
}

static ImU32 TopicColor(console_buffer::Topic topic)
{
    ImU32 color = StyleOf(topic).color;
    return color != 0 ? color : ImGui::GetColorU32(ImGuiCol_Text);
}

// errors and warnings take their color over the topic's
static ImU32 LineColor(const console_buffer::Line& line)
{
    switch (line.severity)
    {
        case console_buffer::Severity::Error:   return kErrorColor;
        case console_buffer::Severity::Warning: return kWarningColor;
        default:                                return TopicColor(line.topic);
    }
}

static ImU32 WithAlpha(ImU32 color, float alpha)
{
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
    c.w = alpha;
    return ImGui::ColorConvertFloat4ToU32(c);
}

void CImGuiConsole::RebuildView()
{
    uint32_t mask = 0;
    for (int topic = 0; topic < console_buffer::kTopicCount; topic++)
    {
        if (m_bTopicVisible[topic])
            mask |= 1u << topic;
    }
    if (m_bProblemsOnly)
        mask |= 1u << 31;
    bool collapse = CvarOn("con_collapse", true);
    if (collapse)
        mask |= 1u << 30;

    bool filterChanged = mask != m_iViewMask || m_ViewSearch != m_szSearch;
    if (!filterChanged && m_Scrollback.Generation() == m_iViewGeneration)
        return;

    m_iViewMask = mask;
    m_ViewSearch = m_szSearch;
    m_iViewGeneration = m_Scrollback.Generation();

    std::string needle = console_buffer::ToLowerUtf8(m_szSearch);
    std::fill(std::begin(m_iTopicCounts), std::end(m_iTopicCounts), 0);
    m_iProblemCount = 0;
    m_View.clear();
    m_ViewRepeats.clear();

    for (const console_buffer::Line& line : m_Scrollback.Lines())
    {
        // where the earlier run ends stays in view whatever the filters
        if (line.divider)
        {
            m_View.push_back(&line);
            m_ViewRepeats.push_back(1);
            continue;
        }

        int topic = static_cast<int>(line.topic);
        bool problem = line.severity != console_buffer::Severity::Normal;
        m_iTopicCounts[topic]++;
        if (problem)
            m_iProblemCount++;

        if (!m_bTopicVisible[topic] || (m_bProblemsOnly && !problem))
            continue;
        if (!needle.empty() && !console_buffer::ContainsLowered(line.text, needle))
            continue;

        // the same line again right after itself only adds to the count of the first
        if (collapse && !m_View.empty() && !line.text.empty() && m_View.back()->text == line.text &&
            m_View.back()->topic == line.topic)
        {
            m_ViewRepeats.back()++;
            continue;
        }

        m_View.push_back(&line);
        m_ViewRepeats.push_back(1);
    }

    // selections are made of rows of the view, which now hold other lines
    if (filterChanged)
        m_SelectionStart = m_SelectionEnd = {};
}

// a toggle colored like what it filters
static void FilterButton(const std::string& label, ImU32 color, bool on, bool first, bool& toggled, bool& soloed)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    float width = ImGui::CalcTextSize(label.c_str(), nullptr, true).x + style.FramePadding.x * 2;

    // wrap the buttons that don't fit on the row
    if (!first && ImGui::GetCursorPosX() + style.ItemSpacing.x + width <= ImGui::GetContentRegionMax().x)
        ImGui::SameLine(0.0f, style.ItemSpacing.x * 0.5f);

    ImGui::PushStyleColor(ImGuiCol_Button, WithAlpha(color, on ? 0.22f : 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(color, 0.35f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, WithAlpha(color, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, on ? color : ImGui::GetColorU32(ImGuiCol_TextDisabled));

    toggled = ImGui::Button(label.c_str());
    soloed = ImGui::IsItemClicked(ImGuiMouseButton_Right);

    ImGui::PopStyleColor(4);
}

void CImGuiConsole::DrawToolbar()
{
    const ImGuiStyle& style = ImGui::GetStyle();
    std::string tip = Localized("#Console_Filter_Tip");
    bool toggled, soloed;

    for (const TopicStyle& topicStyle : kTopicStyles)
    {
        int topic = static_cast<int>(topicStyle.topic);
        std::string label = Localized(topicStyle.label) + " " + std::to_string(m_iTopicCounts[topic]) + "###" + topicStyle.label;

        FilterButton(label, TopicColor(topicStyle.topic), m_bTopicVisible[topic], &topicStyle == &kTopicStyles[0], toggled, soloed);
        if (toggled)
            m_bTopicVisible[topic] = !m_bTopicVisible[topic];

        if (soloed)
        {
            for (bool& other : m_bTopicVisible)
                other = false;
            m_bTopicVisible[topic] = true;
        }

        if (toggled || soloed)
            SaveFilters();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("%s", tip.c_str());
    }

    // across every topic: only the lines that went wrong
    std::string problems = Localized("#Console_Filter_Problems") + " " + std::to_string(m_iProblemCount) + "###Problems";
    FilterButton(problems, kErrorColor, m_bProblemsOnly, false, toggled, soloed);
    if (toggled)
    {
        m_bProblemsOnly = !m_bProblemsOnly;
        SaveFilters();
    }

    const float kSearchMinWidth = 140.0f;
    if (ImGui::GetCursorPosX() + style.ItemSpacing.x + kSearchMinWidth <= ImGui::GetContentRegionMax().x)
        ImGui::SameLine();

    const char* gear = "\u2699";
    float gearWidth = ImGui::CalcTextSize(gear).x + style.FramePadding.x * 2;

    ImGui::SetNextItemWidth(-(gearWidth + style.ItemSpacing.x));
    ImGui::InputTextWithHint("##Search", Localized("#Console_Search").c_str(), m_szSearch, sizeof(m_szSearch));

    ImGui::SameLine();
    if (ImGui::Button(gear, ImVec2(gearWidth, 0)))
        ImGui::OpenPopup("##Settings");
    DrawSettings();
}

void CImGuiConsole::DrawScrollback()
{
    using console_buffer::TextPos;

    RebuildView();

    // console lines sit closer together than the widgets around them
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
    // room on the left for the bar of marked lines
    const float kGutter = 8.0f;
    ImGui::Indent(kGutter);

    ImGuiIO& io = ImGui::GetIO();
    float charWidth = ImGui::GetFont()->GetCharAdvance('M');
    float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    // where row 0 would be, scrolled out of view or not
    ImVec2 rowOrigin = ImGui::GetCursorScreenPos();

    // the timestamps sit in front of the text, outside what a selection counts and copies
    bool timestamps = CvarOn("con_timestamps", false);
    const int kClockChars = 11; // "[10:59:06] "
    ImVec2 origin = rowOrigin;
    if (timestamps)
        origin.x += kClockChars * charWidth;

    auto positionAtMouse = [&]() {
        return console_buffer::PositionAt(m_View, io.MousePos.x - origin.x, io.MousePos.y - origin.y, charWidth, lineHeight);
    };

    // InnerRect leaves out the scrollbar, which has to stay draggable
    bool overText = ImGui::IsWindowHovered() && ImGui::GetCurrentWindow()->InnerRect.Contains(io.MousePos);
    if (overText && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_SelectionStart = m_SelectionEnd = positionAtMouse();
        m_bSelecting = true;
    }

    if (m_bSelecting)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            m_SelectionEnd = positionAtMouse();

            // dragging past the top or bottom edge scrolls on
            float top = ImGui::GetWindowPos().y;
            float bottom = top + ImGui::GetWindowHeight();
            if (io.MousePos.y < top)
                ImGui::SetScrollY(ImGui::GetScrollY() - lineHeight);
            else if (io.MousePos.y > bottom)
                ImGui::SetScrollY(ImGui::GetScrollY() + lineHeight);
        }
        else
        {
            m_bSelecting = false;
        }
    }

    bool hasSelection = m_SelectionStart != m_SelectionEnd;
    TextPos from = std::min(m_SelectionStart, m_SelectionEnd);
    TextPos to = std::max(m_SelectionStart, m_SelectionEnd);

    // the input line handles these itself while it's being typed in
    if (ImGui::IsWindowFocused() && !io.WantTextInput && io.KeyCtrl)
    {
        if (hasSelection && ImGui::IsKeyPressed(ImGuiKey_C, false))
            ImGui::SetClipboardText(console_buffer::SelectedText(m_View, from, to).c_str());

        if (ImGui::IsKeyPressed(ImGuiKey_A, false) && !m_View.empty())
        {
            int last = static_cast<int>(m_View.size()) - 1;
            m_SelectionStart = { 0, 0 };
            m_SelectionEnd = { last, console_buffer::CharCount(console_buffer::LineText(*m_View[last])) };
        }
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 selectionColor = ImGui::GetColorU32(ImGuiCol_TextSelectedBg);
    float rowLeft = ImGui::GetWindowPos().x;
    float rowRight = rowLeft + ImGui::GetWindowWidth();

    // only the rows in view are drawn, the scrollback can hold thousands
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_View.size()), lineHeight);
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            const console_buffer::Line& line = *m_View[i];
            ImU32 lineColor = LineColor(line);
            float top = origin.y + i * lineHeight;

            if (StyleOf(line.topic).marked || line.severity != console_buffer::Severity::Normal)
            {
                drawList->AddRectFilled(ImVec2(rowLeft, top), ImVec2(rowRight, top + lineHeight), WithAlpha(lineColor, 0.08f));
                drawList->AddRectFilled(ImVec2(rowLeft, top), ImVec2(rowLeft + 3.0f, top + lineHeight), lineColor);
            }

            if (hasSelection && i >= from.line && i <= to.line)
            {
                int first = i == from.line ? from.column : 0;
                // a selection going on to the next line also covers this line's break
                int last = i == to.line ? to.column : console_buffer::CharCount(console_buffer::LineText(line)) + 1;

                ImVec2 min(origin.x + first * charWidth, top);
                ImVec2 max(origin.x + last * charWidth, top + lineHeight);
                drawList->AddRectFilled(min, max, selectionColor);
            }

            if (line.divider)
            {
                // ──── previous run · 11:49:51 ────, across the whole row
                std::string label = line.text + " \u00B7 " + console_buffer::FormatClock(line.time);
                ImVec2 size = ImGui::CalcTextSize(label.c_str());
                float middle = top + lineHeight * 0.5f;
                float textLeft = rowLeft + (rowRight - rowLeft - size.x) * 0.5f;
                ImU32 color = ImGui::GetColorU32(ImGuiCol_CheckMark);

                drawList->AddLine(ImVec2(rowLeft + 8.0f, middle), ImVec2(textLeft - charWidth, middle), WithAlpha(color, 0.6f));
                drawList->AddLine(ImVec2(textLeft + size.x + charWidth, middle), ImVec2(rowRight - 8.0f, middle), WithAlpha(color, 0.6f));
                drawList->AddText(ImVec2(textLeft, top), color, label.c_str());

                ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight()));
                continue;
            }

            // lines from an earlier run are a little dimmer than this run's
            float fade = line.previous_session ? 0.8f : 1.0f;

            if (timestamps)
            {
                std::string clock = "[" + console_buffer::FormatClock(line.time) + "] ";
                ImGui::PushStyleColor(ImGuiCol_Text, WithAlpha(ImGui::GetColorU32(ImGuiCol_TextDisabled), 0.8f * fade));
                ImGui::TextUnformatted(clock.c_str());
                ImGui::PopStyleColor();
                ImGui::SameLine(0.0f, 0.0f);
            }

            if (line.segments.empty())
            {
                ImGui::TextUnformatted("");
                continue;
            }

            for (size_t s = 0; s < line.segments.size(); s++)
            {
                const console_buffer::Segment& segment = line.segments[s];
                if (s > 0)
                    ImGui::SameLine(0.0f, 0.0f);

                const auto& c = segment.color;
                ImU32 color = segment.themed ? lineColor : IM_COL32(c.r, c.g, c.b, c.a);
                if (fade < 1.0f)
                    color = WithAlpha(color, ImGui::ColorConvertU32ToFloat4(color).w * fade);

                // typed commands start with "] ", shown as a prompt mark of the same width
                std::string_view text = segment.text;
                std::string shown;
                if (s == 0 && line.topic == console_buffer::Topic::Commands && text.starts_with("] "))
                {
                    shown = "›";
                    shown += text.substr(1);
                    text = shown;
                }

                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(text.data(), text.data() + text.size());
                ImGui::PopStyleColor();
            }

            if (m_ViewRepeats[i] > 1)
            {
                std::string count = "\u00D7" + std::to_string(m_ViewRepeats[i]);
                ImGui::SameLine(0.0f, charWidth);
                ImGui::PushStyleColor(ImGuiCol_Text, WithAlpha(lineColor, 0.7f));
                ImGui::TextUnformatted(count.c_str());
                ImGui::PopStyleColor();
            }
        }
    }

    // follow new output, unless the player scrolled up to read something
    if (!m_bSelecting && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::Unindent(kGutter);
    ImGui::PopStyleVar();
}

void CImGuiConsole::Execute(const char* command)
{
    m_History.Add(command);
    SaveHistory();

    GameConsole().Printf("] %s\n", command);
    engine->pfnClientCmd(command);
}

void CImGuiConsole::OnKeyCodeTyped(KeyCode code)
{
    KeyCode consoleCode = g_pGameUIFuncs->GetVGUI2KeyCodeForBind("toggleconsole");
    bool shiftDown = input()->IsKeyDown(KEY_LSHIFT) || input()->IsKeyDown(KEY_RSHIFT);

    // same fix for the russian keyboard layout as in CGameConsoleDialog::OnKeyCodeTyped
#ifdef _WIN32
    if (code == KEY_NONE && consoleCode == KEY_NONE && ::GetKeyState(VK_OEM_3) & 0x8000)
    {
        code = KEY_BACKQUOTE;
        consoleCode = KEY_BACKQUOTE;
    }
#endif

    if (code != KEY_NONE && code == consoleCode && !shiftDown)
    {
        // the console key also types its character, which shouldn't end up in the input line
        m_bIgnoreNextChar = true;
        CloseToGame();
    }
}

void CImGuiConsole::OnKeyTyped(wchar_t unichar)
{
    if (m_bIgnoreNextChar)
    {
        m_bIgnoreNextChar = false;
        return;
    }

    BaseClass::OnKeyTyped(unichar);
}

void CImGuiConsole::CloseToGame()
{
    SetVisible(false);

    if (!g_pBaseUI)
        return;

    if (LoadingDialog())
        surface()->RestrictPaintToSinglePanel(LoadingDialog()->GetVPanel());
    else
        g_pBaseUI->HideGameUI();
}
