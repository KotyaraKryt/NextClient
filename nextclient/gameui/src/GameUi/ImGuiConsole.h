#pragma once

#include "ImGuiPanel.h"

#include <console_buffer/command_history.h>
#include <console_buffer/selection.h>

class CImGuiConsole : public CImGuiPanel
{
    DECLARE_CLASS_SIMPLE(CImGuiConsole, CImGuiPanel);

public:
    explicit CImGuiConsole(console_buffer::ConsoleBuffer& scrollback);

    void Activate();

protected:
    void DrawImGui() override;

    void OnKeyCodeTyped(vgui2::KeyCode code) override;
    void OnKeyTyped(wchar_t unichar) override;

private:
    static int OnInputCallback(struct ImGuiInputTextCallbackData* data);

    void DrawScrollback();
    void LoadHistory();
    void SaveHistory();
    void Execute(const char* command);
    void CloseToGame();

    console_buffer::ConsoleBuffer& m_Scrollback;
    console_buffer::CommandHistory m_History;
    char m_szInput[256] = {};
    bool m_bFocusWindow = false;
    bool m_bFocusInput = false;
    bool m_bIgnoreNextChar = false;

    console_buffer::TextPos m_SelectionStart;
    console_buffer::TextPos m_SelectionEnd;
    bool m_bSelecting = false;
};
