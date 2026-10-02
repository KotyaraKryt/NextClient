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

void CImGuiQueryBox::Show(const std::string& title, const std::string& text, const std::string& okText, std::function<void()> onOk)
{
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
    ImGui::GetWindowDrawList()->AddRectFilled(viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), IM_COL32(0, 0, 0, 140));
    ImGui::End();

    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Always);
    if (m_bFocusWindow)
    {
        ImGui::SetNextWindowFocus();
        m_bFocusWindow = false;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22, 20));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kRounding * 1.5f);

    bool ok = false;
    bool cancel = false;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("###Query", nullptr, flags))
    {
        ImGui::PushFont(HeadingFont());
        ImGui::TextUnformatted(m_Title.c_str());
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, 4));

        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(m_Text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0, 12));

        const ImGuiStyle& style = ImGui::GetStyle();
        std::string cancelText = Localized("#GameUI_Cancel");
        float buttonWidth = 96.0f;
        for (const std::string* text : { &m_OkText, &cancelText })
            buttonWidth = std::max(buttonWidth, ImGui::CalcTextSize(text->c_str()).x + style.FramePadding.x * 2.0f);

        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - buttonWidth * 2.0f - style.ItemSpacing.x);
        cancel = ImGui::Button(cancelText.c_str(), ImVec2(buttonWidth, 0));

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(ImGuiCol_CheckMark, 0.85f));
        ok = ImGui::Button(m_OkText.c_str(), ImVec2(buttonWidth, 0));
        ImGui::PopStyleColor(2);

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            cancel = true;
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
            ok = true;
    }
    ImGui::End();

    ImGui::PopStyleVar(2);

    if (ok)
    {
        Close();
        if (m_OnOk)
            m_OnOk();
    }
    else if (cancel)
        Close();
}
