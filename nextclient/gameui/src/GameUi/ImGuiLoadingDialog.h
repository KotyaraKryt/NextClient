#pragma once

#include "ImGuiPanel.h"

#include <imgui/imgui.h>
#include "LoadingDialog.h"

#include <string>

// The connection and level loading screen drawn with Dear ImGui over the menu's background;
// loading_legacy 1 brings the VGUI dialog back. The panel is kept between connections,
// IsOpen says whether a loading is going on
class CImGuiLoadingDialog : public CImGuiPanel, public ILoadingDialog
{
    DECLARE_CLASS_SIMPLE(CImGuiLoadingDialog, CImGuiPanel);

public:
    CImGuiLoadingDialog();

    static void RegisterCvars();
    static bool UseLegacyDialog();

    bool IsOpen() const { return m_bOpen; }
    // a fresh loading, waiting for Open or an error
    void Reset();

    void Open(bool bShowBackground = false) override;
    bool SetProgressPoint(int progressPoint) override;
    void SetProgressRange(int min, int max) override;
    void SetStatusText(const char* statusText) override;
    void SetSecondaryProgress(float progress) override;
    void SetSecondaryProgressText(const char* statusText) override;
    void DisplayGenericError(const char* failureReason, const char* extendedReason = nullptr) override;
    void SetBackgroundImage(const char* imageName) override {}
    void ShowLoading() override;
    void CloseLoading() override;
    vgui2::VPANEL GetLoadingPanel() override { return GetVPanel(); }

    // the map being loaded, shown under the title
    void SetLevelName(const char* levelName) override;

protected:
    void Paint() override;
    void DrawImGui() override;
    void PreparePreview() override;

    // while loading the engine only draws a frame when the progress moves, seconds apart, and
    // ImGui would only see a click in the next one; Cancel answers VGUI's events right away instead
    void OnMousePressed(vgui2::MouseCode code) override;
    void OnMouseReleased(vgui2::MouseCode code) override;
    void OnKeyCodePressed(vgui2::KeyCode code) override;

private:
    void DrawProgressBar(float fraction, float width, float height);
    void Cancel();
    bool IsOverCancel() const;

    bool m_bOpen = false;
    bool m_bError = false;

    int m_iRangeMin = 0;
    int m_iRangeMax = 0;
    float m_flProgress = 0.0f;
    std::string m_Status;
    std::string m_Level;

    // a download's progress, shown under the main bar once it starts
    bool m_bShowingSecondary = false;
    float m_flSecondary = 0.0f;
    float m_flSecondaryStartTime = 0.0f;
    float m_flSecondaryUpdateTime = 0.0f;
    std::string m_SecondaryText;

    std::string m_ErrorText;
    // the text block's height in the last frame, to stand it on the bottom margin
    float m_flBlockHeight = 0.0f;
    // where the last frame drew the Cancel button, in screen space
    ImVec2 m_CancelMin;
    ImVec2 m_CancelMax;
    bool m_bCancelPressed = false;
};
