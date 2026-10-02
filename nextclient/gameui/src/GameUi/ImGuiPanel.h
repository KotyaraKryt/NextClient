#pragma once

#include <vgui_controls/Panel.h>

#include <string>

struct ImFont;
struct ImGuiContext;

// A VGUI popup that draws Dear ImGui with raw OpenGL from inside Paint(), so the
// text goes through ImGui's own font atlas instead of the engine's font code
class CImGuiPanel : public vgui2::Panel
{
    DECLARE_CLASS_SIMPLE(CImGuiPanel, vgui2::Panel);

public:
    // layoutFile keeps where the windows were and how big, between runs; titleFontScale > 0
    // adds a third font that many times the normal size, for TitleFont
    explicit CImGuiPanel(const char* layoutFile = nullptr, float titleFontScale = 0.0f);

    // takes effect on the next frame, when the font atlas can be rebuilt
    void SetFontSize(float size);

    // a localization token in UTF-8, "" when no loaded file has it
    static std::string Localized(const char* token);
    // for the tokens only NextClient's own files have, which not every language has yet
    static std::string Localized(const char* token, const char* english);

    // the bigger font of the current context, nullptr (which ImGui::PushFont takes as the default) if it failed to load
    static ImFont* HeadingFont();
    // the panel's title font, or the heading one if it asked for none
    static ImFont* TitleFont();

    ~CImGuiPanel() override;

protected:
    // called between ImGui::NewFrame and ImGui::Render, with this panel's context current
    virtual void DrawImGui() = 0;

    void Paint() override;

    // forget held keys and buttons: their releases may have gone to another panel
    void ResetInput();

    void OnSetFocus() override;
    void OnKillFocus() override;

    void OnCursorMoved(int x, int y) override;
    void OnMousePressed(vgui2::MouseCode code) override;
    void OnMouseDoublePressed(vgui2::MouseCode code) override;
    void OnMouseReleased(vgui2::MouseCode code) override;
    void OnMouseWheeled(int delta) override;
    void OnCursorExited() override;
    void OnKeyCodePressed(vgui2::KeyCode code) override;
    void OnKeyCodeReleased(vgui2::KeyCode code) override;
    void OnKeyCodeTyped(vgui2::KeyCode code) override;
    void OnKeyTyped(wchar_t unichar) override;

private:
    void CreateFontTexture();
    void FitToWindows();
    void KeepWindowsOnScreen();
    void SaveLayout();
    void ReleaseKeysLetGoElsewhere();
    void OnKey(vgui2::KeyCode code, bool down);

    ImGuiContext* m_pContext;
    int m_iFontTextureID = 0;
    double m_flLastFrameTime = 0.0;
    float m_flFontSize = 16.0f;
    float m_flTitleFontScale = 0.0f;
    float m_flPendingFontSize = 0.0f;
    const char* m_pszLayoutFile;
};
