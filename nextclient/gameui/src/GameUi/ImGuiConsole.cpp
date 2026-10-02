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

CImGuiConsole::CImGuiConsole(console_buffer::ConsoleBuffer& scrollback) : m_Scrollback(scrollback)
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

// the description from resource/console_<language>.txt, "" for names it doesn't cover
const std::string& CImGuiConsole::Describe(const std::string& name)
{
    auto found = m_Descriptions.find(name);
    if (found != m_Descriptions.end())
        return found->second;

    std::string utf8;
    std::string token = "#Console_Help_" + name;
    if (const wchar_t* wide = g_pVGuiLocalize->Find(token.c_str()))
    {
        utf8.resize(wcslen(wide) * 4 + 1);
        V_UnicodeToUTF8(wide, utf8.data(), static_cast<int>(utf8.size()));
        utf8.resize(strlen(utf8.c_str()));
    }

    return m_Descriptions.emplace(name, std::move(utf8)).first->second;
}

void CImGuiConsole::UpdateSuggestions()
{
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

    bool open = true;
    bool expanded = ImGui::Begin("Console", &open, ImGuiWindowFlags_NoCollapse);

    if (expanded)
    {
        // leave one row under the scrollback for the input line
        float footer = ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("Scrollback", ImVec2(0, -footer));
        DrawScrollback();
        ImGui::EndChild();

        // typing after selecting something in the scrollback goes back to the input line
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput && !io.InputQueueCharacters.empty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
            m_bFocusInput = true;

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
}

void CImGuiConsole::DrawScrollback()
{
    using console_buffer::TextPos;

    // console lines sit closer together than the widgets around them
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));

    ImGuiIO& io = ImGui::GetIO();
    float charWidth = ImGui::GetFont()->GetCharAdvance('M');
    float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    // where line 0 would be, scrolled out of view or not
    ImVec2 origin = ImGui::GetCursorScreenPos();

    auto positionAtMouse = [&]() {
        return console_buffer::PositionAt(m_Scrollback, io.MousePos.x - origin.x, io.MousePos.y - origin.y, charWidth, lineHeight);
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
            ImGui::SetClipboardText(console_buffer::SelectedText(m_Scrollback, from, to).c_str());

        if (ImGui::IsKeyPressed(ImGuiKey_A, false) && !m_Scrollback.Lines().empty())
        {
            int last = static_cast<int>(m_Scrollback.Lines().size()) - 1;
            m_SelectionStart = { 0, 0 };
            m_SelectionEnd = { last, console_buffer::CharCount(console_buffer::LineText(m_Scrollback.Lines()[last])) };
        }
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 selectionColor = ImGui::GetColorU32(ImGuiCol_TextSelectedBg);

    // only the lines in view are drawn, the scrollback can hold thousands
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_Scrollback.Lines().size()), lineHeight);
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            const console_buffer::Line& line = m_Scrollback.Lines()[i];

            if (hasSelection && i >= from.line && i <= to.line)
            {
                int first = i == from.line ? from.column : 0;
                // a selection going on to the next line also covers this line's break
                int last = i == to.line ? to.column : console_buffer::CharCount(console_buffer::LineText(line)) + 1;

                ImVec2 min(origin.x + first * charWidth, origin.y + i * lineHeight);
                ImVec2 max(origin.x + last * charWidth, min.y + lineHeight);
                drawList->AddRectFilled(min, max, selectionColor);
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
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(c.r, c.g, c.b, c.a));
                ImGui::TextUnformatted(segment.text.data(), segment.text.data() + segment.text.size());
                ImGui::PopStyleColor();
            }
        }
    }

    // follow new output, unless the player scrolled up to read something
    if (!m_bSelecting && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

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
