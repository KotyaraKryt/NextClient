#pragma once

#include "ImGuiPanel.h"

#include <imgui/imgui.h>

#include <string>
#include <vector>

class IDemoPlayer;
class IEngineWrapper;
class IWorld;

// The HLTV demo player (demoui) drawn with Dear ImGui; demoui_legacy 1 brings the VGUI one back.
// It hangs off the root panel rather than the menu, so it stays up while the demo plays
class CImGuiDemoPlayer : public CImGuiPanel
{
    DECLARE_CLASS_SIMPLE(CImGuiDemoPlayer, CImGuiPanel);

public:
    CImGuiDemoPlayer();

    static void RegisterCvars();
    static bool UseLegacyDialog();

    void Activate();
    void Close();
    // starts the demo with this file name, the way the old dialog's file list did
    void DemoSelected(const char* demoName);

protected:
    void DrawImGui() override;

private:
    bool LoadModules();
    void LoadDemoList();
    void DrawTransport();
    void DrawLoadPopup();

    void Play();
    void Pause();
    void Step(int direction);
    void GoToStart();
    void GoToEnd();
    void ChangeSpeed(bool faster);
    void Seek(float seconds);
    void Stop();
    void DrawBottomRow();
    void DrawTimeSlider(float width);
    // the one-row player demoui_compact asks for
    void DrawCompact();
    void HandleKeys();
    void AdvanceBeyondModuleSpeed();
    std::string SpeedText() const;
    // the widths the rows need, so the window can be as wide as the widest
    float TransportWidth() const;
    float ContentWidth() const;
    // fades the window out while the demo plays and the mouse rests, and back in when it moves
    void UpdateAutoHide();

    // the demo's first and last frame times, past the frames recorded before it really starts
    bool GetTimeRange(float& start, float& end);

    IEngineWrapper* m_pEngine = nullptr;
    IDemoPlayer* m_pDemoPlayer = nullptr;
    IWorld* m_pWorld = nullptr;

    bool m_bFocusWindow = false;
    bool m_bOpenLoadPopup = false;
    std::vector<std::string> m_Demos;
    std::string m_SelectedDemo;
    char m_szSearch[64] = {};

    // the cursor where it was last seen, and when it or a button last moved
    int m_iLastCursorX = -1;
    int m_iLastCursorY = -1;
    double m_flLastActivity = 0.0;
    double m_flLastFrame = 0.0;
    float m_flAlpha = 1.0f;

    // the speed chosen, which may be beyond what the module plays at by itself
    float m_flSpeed = 1.0f;
    double m_flLastAdvance = 0.0;

    // the window's bottom middle, which stays put when it switches between full and compact
    ImVec2 m_BottomCenter;
    bool m_bHaveAnchor = false;
    bool m_bWasCompact = false;
    int m_iAnchorFrames = 0;
};
