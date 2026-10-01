#pragma once

#include <vgui_controls/Panel.h>

struct ImGuiContext;

// A VGUI popup that draws Dear ImGui with raw OpenGL from inside Paint(), so the
// text goes through ImGui's own font atlas instead of the engine's font code
class CImGuiPanel : public vgui2::Panel
{
    DECLARE_CLASS_SIMPLE(CImGuiPanel, vgui2::Panel);

public:
    CImGuiPanel();
    ~CImGuiPanel() override;

protected:
    void Paint() override;

    void OnCursorMoved(int x, int y) override;
    void OnMousePressed(vgui2::MouseCode code) override;
    void OnMouseDoublePressed(vgui2::MouseCode code) override;
    void OnMouseReleased(vgui2::MouseCode code) override;
    void OnMouseWheeled(int delta) override;
    void OnKeyCodePressed(vgui2::KeyCode code) override;
    void OnKeyCodeReleased(vgui2::KeyCode code) override;
    void OnKeyTyped(wchar_t unichar) override;

private:
    void CreateFontTexture();
    void OnKey(vgui2::KeyCode code, bool down);

    ImGuiContext* m_pContext;
    int m_iFontTextureID = 0;
    double m_flLastFrameTime = 0.0;
};
