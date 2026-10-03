#pragma once

#include "ImGuiAppearance.h"

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
    // which of the Interface page's windows this is: its opacity and text size follow the page's settings
    void SetAppearance(ImGuiAppearance::Element element);

    // makes this a panel only the options' Interface page draws, in a small frame: never shown, with
    // sample content, and none of what the window does when it's used
    void MakePreview();
    bool IsPreview() const { return m_bPreview; }
    // one frame laid out for the whole screen with these settings; the windows' rectangle, the
    // dimmed backdrop left out, goes to windowsMin and windowsMax. Valid until the next call
    ImDrawData* RenderPreview(const ImGuiAppearance::Values& values, ImVec2& windowsMin, ImVec2& windowsMax);
    // draws another context's frame at this point of drawList, scaled by scale, moved by offset and kept within clip
    static void AddScaledDrawData(ImDrawList* drawList, ImDrawData* data, const ImVec2& offset, float scale, const ImVec4& clip);

    // a localization token in UTF-8, "" when no loaded file has it
    static std::string Localized(const char* token);
    // for the tokens only NextClient's own files have, which not every language has yet
    static std::string Localized(const char* token, const char* english);

    // the bigger font of the current context, nullptr (which ImGui::PushFont takes as the default) if it failed to load
    static ImFont* HeadingFont();
    // the panel's title font, or the heading one if it asked for none
    static ImFont* TitleFont();
    // size, but no bigger than the screen: for the windows' first and smallest sizes, which were
    // picked for 1080p and would push a window's bottom off a 640x480 screen
    static ImVec2 OnScreen(const ImVec2& size);

    ~CImGuiPanel() override;

protected:
    // called between ImGui::NewFrame and ImGui::Render, with this panel's context current
    virtual void DrawImGui() = 0;
    // fills a preview's window with something to show
    virtual void PreparePreview() {}

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
    void ApplyAppearance();
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
    ImGuiAppearance::Element m_Appearance = ImGuiAppearance::Element::Count;
    bool m_bPreview = false;
    // the theme's colours, which the opacity is applied to anew each frame
    ImVec4 m_BaseColors[ImGuiCol_COUNT];
};
