#pragma once

#include "ImGuiPanel.h"

#include <functional>
#include <string>

// A question over a dimmed screen, like vgui2::QueryBox: nothing behind it takes clicks until it's answered
class CImGuiQueryBox : public CImGuiPanel
{
    DECLARE_CLASS_SIMPLE(CImGuiQueryBox, CImGuiPanel);

public:
    CImGuiQueryBox();

    // the texts are UTF-8; Escape cancels, Enter confirms
    void Show(const std::string& title, const std::string& text, const std::string& okText, std::function<void()> onOk);
    void Close();

protected:
    void DrawImGui() override;

private:
    std::string m_Title;
    std::string m_Text;
    std::string m_OkText;
    std::function<void()> m_OnOk;
    bool m_bFocusWindow = false;
};
