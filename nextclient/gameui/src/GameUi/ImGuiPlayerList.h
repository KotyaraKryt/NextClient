#pragma once

#include "ImGuiPanel.h"

#include <string>

// The in-game player list drawn with Dear ImGui; plist_legacy 1 brings the VGUI one back.
// It reads the players from the engine every frame, so it stays current while it's open
class CImGuiPlayerList : public CImGuiPanel
{
    DECLARE_CLASS_SIMPLE(CImGuiPlayerList, CImGuiPanel);

public:
    CImGuiPlayerList();

    static void RegisterCvars();
    static bool UseLegacyDialog();

    void Activate();
    void Close();

protected:
    void DrawImGui() override;

private:
    std::string Title() const;

    char m_szSearch[64] = {};
    bool m_bFocusWindow = false;
};
