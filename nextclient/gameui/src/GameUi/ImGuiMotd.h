#pragma once

#include "ImGuiPanel.h"
#include "MotdText.h"

#include <next_gameui/IMotdNext.h>
#include <vgui_controls/PHandle.h>

namespace vgui2
{
    class HTML;
}

#include <imgui/imgui.h>

#include <string>

// The server's message of the day in the windows' style. Plain text is drawn by ImGui; a page
// (HTML or a link) goes to the engine's browser, the one the client's own window uses, put where
// the text would be. Servers check that their page was loaded, so it has to be a real browser.
// motd_legacy 1 brings the client's own window back
class CImGuiMotd : public CImGuiPanel
{
    DECLARE_CLASS_SIMPLE(CImGuiMotd, CImGuiPanel);

public:
    CImGuiMotd();

    void Show(const char* text);
    void Close();

protected:
    void DrawImGui() override;
    void PreparePreview() override;
    void OnThink() override;

private:
    void DrawBody();
    void DrawLinks();
    // the browser over the dark panel, which is at min..max on the screen
    void PlacePage(const ImVec2& min, const ImVec2& max);
    void LoadPage();
    // the MOTD as the server sent it, in a browser: its link, or the HTML from a temporary file
    void OpenInBrowser();

    std::string m_Raw;
    MotdText m_Text;
    bool m_bFocusWindow = false;
    // the MOTD is a page and the browser shows it, not the text
    bool m_bPage = false;
    vgui2::HTML* m_pHtml = nullptr;
    // how tall the text was last frame, so the window fits it
    float m_flBodyHeight = 0.0f;
    ImVec2 m_WindowMin;
    ImVec2 m_WindowMax;
};

class CMotdNext : public IMotdNext
{
public:
    static void RegisterCvars();

    bool IsEnabled() override;
    void Show(const char* text) override;
    bool IsVisible() override;
    void Hide() override;

private:
    vgui2::DHANDLE<CImGuiMotd> m_hPanel;
};
