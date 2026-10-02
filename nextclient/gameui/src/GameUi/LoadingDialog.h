#ifndef LOADINGDIALOG_H
#define LOADINGDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

class CBitmapImagePanel;

// What the engine's loading callbacks drive: the old VGUI dialog or the ImGui one (loading_legacy picks)
class ILoadingDialog
{
public:
    virtual ~ILoadingDialog() = default;

    virtual void Open(bool bShowBackground = false) = 0;
    // true when the bar visibly moved, so the engine knows to redraw
    virtual bool SetProgressPoint(int progressPoint) = 0;
    virtual void SetProgressRange(int min, int max) = 0;
    virtual void SetStatusText(const char *statusText) = 0;
    virtual void SetSecondaryProgress(float progress) = 0;
    virtual void SetSecondaryProgressText(const char *statusText) = 0;
    virtual void DisplayGenericError(const char *failureReason, const char *extendedReason = NULL) = 0;
    virtual void SetBackgroundImage(const char *imageName) = 0;
    virtual void SetLevelName(const char *levelName) {}
    // brings the dialog up again and paints only it
    virtual void ShowLoading() = 0;
    virtual void CloseLoading() = 0;
    // the panel the console hands painting back to when it closes during loading
    virtual vgui2::VPANEL GetLoadingPanel() = 0;
};

class CLoadingDialog : public vgui2::Frame, public ILoadingDialog
{
    DECLARE_CLASS_SIMPLE(CLoadingDialog, vgui2::Frame);

public:
    CLoadingDialog(vgui2::Panel *parent);
    ~CLoadingDialog(void) override;

public:
    void Open(bool bShowBackground = false) override;
    bool SetProgressPoint(int progressPoint) override;
    void SetProgressRange(int min, int max) override;
    void SetStatusText(const char *statusText) override;
    void SetSecondaryProgress(float progress) override;
    void SetSecondaryProgressText(const char *statusText) override;
    void DisplayGenericError(const char *failureReason, const char *extendedReason = NULL) override;
    void SetBackgroundImage(const char *imageName) override;
    void ShowLoading() override { Activate(); }
    void CloseLoading() override { Close(); }
    vgui2::VPANEL GetLoadingPanel() override { return GetVPanel(); }
    void Activate(void) override;

protected:
    virtual void OnCommand(const char *command);
    virtual void PerformLayout(void);
    virtual void OnThink(void);
    virtual void OnClose(void);
    virtual void OnKeyCodePressed(vgui2::KeyCode code);
    virtual void PaintBackground(void);

private:
    void SetupControlSettings(bool bForceShowProgressText);
    void SetupControlSettingsForErrorDisplay(const char *settingsFile);

public:
    vgui2::Panel *m_pParent;
    vgui2::ProgressBar *m_pProgress;
    vgui2::ProgressBar *m_pProgress2;
    vgui2::Label *m_pInfoLabel;
    vgui2::Label *m_pTimeRemainingLabel;
    vgui2::Button *m_pCancelButton;
    static CBitmapImagePanel *m_pLoadingBackground;
    int m_iRangeMin, m_iRangeMax;
    bool m_bShowingSecondaryProgress;
    float m_flSecondaryProgress;
    float m_flLastSecondaryProgressUpdateTime;
    float m_flSecondaryProgressStartTime;
    bool m_bCenter;
    float m_flProgressFraction;
    char m_szBackgroundImage[MAX_PATH];
    bool m_bShowBackground;
    vgui2::VPANEL m_RestrictPanel;

public:
    CPanelAnimationVar(int, m_iAdditionalIndentX, "AdditionalIndentX", "0");
    CPanelAnimationVar(int, m_iAdditionalIndentY, "AdditionalIndentY", "0");
};

// the loading dialog while there is one, nullptr otherwise
ILoadingDialog *LoadingDialog(void);
// the current one, or a new one of the kind loading_legacy asks for
ILoadingDialog *CreateLoadingDialog(void);
void CloseLoadingDialog(void);
#endif