#include "GameUi.h"
#include "ImGuiConsole.h"
#include "GameConsole.h"
#include "IBaseUI.h"
#include "IGameUIFuncs.h"
#include "LoadingDialog.h"
#include <FileSystem.h>

#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Controls.h>

#include <console_buffer/console_buffer.h>
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

int CImGuiConsole::OnInputCallback(ImGuiInputTextCallbackData* data)
{
    auto* console = static_cast<CImGuiConsole*>(data->UserData);

    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
    {
        std::optional<std::string> entry = data->EventKey == ImGuiKey_UpArrow ? console->m_History.Older() : console->m_History.Newer();
        if (entry)
        {
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, entry->c_str());
        }
    }

    return 0;
}

void CImGuiConsole::Activate()
{
    SetVisible(true);
    MoveToFront();
    RequestFocus();

    ResetInput();
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
        }

        const char* submitLabel = "Submit";
        float submitWidth = ImGui::CalcTextSize(submitLabel).x + ImGui::GetStyle().FramePadding.x * 2;

        ImGui::SetNextItemWidth(-(submitWidth + ImGui::GetStyle().ItemSpacing.x));
        bool submitted = ImGui::InputText("##Input", m_szInput, sizeof(m_szInput),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory, OnInputCallback, this);
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
