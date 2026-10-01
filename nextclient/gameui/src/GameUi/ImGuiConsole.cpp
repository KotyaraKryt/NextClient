#include "GameUi.h"
#include "ImGuiConsole.h"
#include "GameConsole.h"
#include "IBaseUI.h"
#include "IGameUIFuncs.h"
#include "LoadingDialog.h"

#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Controls.h>

#include <console_buffer/console_buffer.h>
#include <imgui/imgui.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <cfloat>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui2;

CImGuiConsole::CImGuiConsole(console_buffer::ConsoleBuffer& scrollback) : m_Scrollback(scrollback)
{
    SetVisible(false);
}

void CImGuiConsole::Activate()
{
    SetVisible(true);
    MoveToFront();
    RequestFocus();

    m_bFocusInput = true;
    m_bIgnoreNextChar = false;
}

void CImGuiConsole::DrawImGui()
{
    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(720, 420), ImGuiCond_FirstUseEver);

    bool open = true;
    bool expanded = ImGui::Begin("Console", &open);

    if (expanded)
    {
        // leave one row under the scrollback for the input line
        float footer = ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("Scrollback", ImVec2(0, -footer));

        // only the lines in view are drawn, the scrollback can hold thousands
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(m_Scrollback.Lines().size()));
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                const console_buffer::Line& line = m_Scrollback.Lines()[i];
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
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();

        if (m_bFocusInput)
        {
            ImGui::SetKeyboardFocusHere();
            m_bFocusInput = false;
        }

        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("##Input", m_szInput, sizeof(m_szInput), ImGuiInputTextFlags_EnterReturnsTrue))
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

void CImGuiConsole::Execute(const char* command)
{
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
