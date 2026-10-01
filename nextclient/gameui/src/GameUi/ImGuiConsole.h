#pragma once

#include "ImGuiPanel.h"

namespace console_buffer
{
    class ConsoleBuffer;
}

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
    void Execute(const char* command);
    void CloseToGame();

    console_buffer::ConsoleBuffer& m_Scrollback;
    char m_szInput[256] = {};
    bool m_bFocusInput = false;
    bool m_bIgnoreNextChar = false;
};
