#include "ImGuiQueryBox.h"
#include "ImGuiForm.h"

#include <imgui/imgui.h>

#include <algorithm>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace ImGuiForm;

CImGuiQueryBox::CImGuiQueryBox()
{
    SetVisible(false);
}

void CImGuiQueryBox::Show(const std::string& title, const std::string& text, const std::string& okText, std::function<void()> onOk, bool cancelButton)
{
    m_bCancelButton = cancelButton;
    m_Title = title;
    m_Text = text;
    m_OkText = okText;
    m_OnOk = std::move(onOk);

    SetVisible(true);
    MoveToFront();
    RequestFocus();
    ResetInput();
    m_bFocusWindow = true;
}

void CImGuiQueryBox::Close()
{
    SetVisible(false);
}

void CImGuiQueryBox::DrawImGui()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    // a window over the whole screen, so the panel covers it and the menu behind gets no clicks
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGuiWindowFlags backdropFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##Backdrop", nullptr, backdropFlags);
    ImDrawList* backdrop = ImGui::GetWindowDrawList();
    backdrop->AddRectFilled(viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), IM_COL32(0, 0, 0, 150));

    // ImGui has no shadows: a soft one from rings that grow and fade, under where the window was last frame
    if (m_WindowMax.x > m_WindowMin.x)
    {
        constexpr int kLayers = 14;
        for (int i = kLayers; i >= 1; i--)
        {
            float spread = static_cast<float>(i) * 2.0f;
            int alpha = static_cast<int>(10.0f * (1.0f - static_cast<float>(i - 1) / kLayers));
            backdrop->AddRectFilled(ImVec2(m_WindowMin.x - spread, m_WindowMin.y - spread + 6.0f), ImVec2(m_WindowMax.x + spread, m_WindowMax.y + spread + 6.0f),
                IM_COL32(0, 0, 0, alpha), kRounding + spread);
        }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Always);
    if (m_bFocusWindow)
    {
        ImGui::SetNextWindowFocus();
        m_bFocusWindow = false;
    }

    bool ok = false;
    bool cancel = false;
    bool open = true;

    // a window like the options and the browser: the same title bar and close cross, the buttons on the right
    std::string title = m_Title + "###Query";
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin(title.c_str(), &open, flags))
    {
        const ImGuiStyle& style = ImGui::GetStyle();

        // the text on the dark panel the console and the browser keep their content on, the buttons under it
        constexpr float kBodyPadding = 14.0f;
        float textWidth = ImGui::GetContentRegionAvail().x - kBodyPadding * 2.0f;
        float bodyHeight = ImGui::CalcTextSize(m_Text.c_str(), nullptr, false, textWidth).y + kBodyPadding * 2.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kBodyPadding, kBodyPadding));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kRounding);
        ImGui::BeginChild("Body", ImVec2(0, bodyHeight), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(m_Text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::Dummy(ImVec2(0, style.ItemSpacing.y));

        std::string cancelText = Localized("#GameUI_Cancel");
        float buttonWidth = 96.0f;
        for (const std::string* text : { &m_OkText, &cancelText })
            buttonWidth = std::max(buttonWidth, ImGui::CalcTextSize(text->c_str()).x + style.FramePadding.x * 2.0f);

        int buttons = m_bCancelButton ? 2 : 1;
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - buttonWidth * buttons - style.ItemSpacing.x * (buttons - 1));
        if (m_bCancelButton)
        {
            cancel = ImGui::Button(cancelText.c_str(), ImVec2(buttonWidth, 0));
            ImGui::SameLine();
        }

        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(ImGuiCol_CheckMark, 0.85f));
        ok = ImGui::Button(m_OkText.c_str(), ImVec2(buttonWidth, 0));
        ImGui::PopStyleColor(2);

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            cancel = true;
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
            ok = true;

        m_WindowMin = ImGui::GetWindowPos();
        m_WindowMax = ImVec2(m_WindowMin.x + ImGui::GetWindowWidth(), m_WindowMin.y + ImGui::GetWindowHeight());
    }
    ImGui::End();

    if (!open)
        cancel = true;

    if (ok)
    {
        Close();
        if (m_OnOk)
            m_OnOk();
    }
    else if (cancel)
        Close();
}
