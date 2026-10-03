#include "ImGuiLoadingDialog.h"
#include "BasePanel.h"
#include "GameUi.h"
#include "IGameUIFuncs.h"

#include <vgui/ILocalize.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/ISystem.h>
#include <vgui/IInputInternal.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/ProgressBar.h>

#include <cvardef.h>
#include <tier1/strtools.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cmath>
#include <cwchar>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

namespace
{
    cvar_t* g_pLegacyCvar = nullptr;

    // the title font's size against the normal one
    constexpr float kTitleScale = 3.0f;

    // the engine sends tokens and plain text alike
    std::string LocalizedText(const char* text)
    {
        if (text[0] == '#')
        {
            std::string localized = CImGuiPanel::Localized(text);
            if (!localized.empty())
                return localized;
        }
        return text;
    }

    std::string ToUTF8(const wchar_t* wide)
    {
        std::string utf8(wcslen(wide) * 4 + 1, '\0');
        V_UnicodeToUTF8(wide, utf8.data(), static_cast<int>(utf8.size()));
        utf8.resize(strlen(utf8.c_str()));
        return utf8;
    }

    void ToWide(const char* text, wchar_t* out, int outBytes)
    {
        const wchar_t* found = text[0] == '#' ? g_pVGuiLocalize->Find(text) : nullptr;
        if (found)
            V_wcsncpy(out, found, outBytes);
        else
            V_UTF8ToUnicode(text, out, outBytes);
    }

    float Now()
    {
        return static_cast<float>(vgui2::system()->GetFrameTime());
    }
}

CImGuiLoadingDialog::CImGuiLoadingDialog() : BaseClass(nullptr, kTitleScale)
{
    SetAppearance(ImGuiAppearance::Element::Loading);
    SetVisible(false);
}

void CImGuiLoadingDialog::RegisterCvars()
{
    g_pLegacyCvar = engine->pfnRegisterVariable("loading_legacy", "0", FCVAR_ARCHIVE);
}

bool CImGuiLoadingDialog::UseLegacyDialog()
{
    return g_pLegacyCvar && g_pLegacyCvar->value != 0.0f;
}

void CImGuiLoadingDialog::Reset()
{
    m_bOpen = true;
    m_bError = false;
    m_iRangeMin = 0;
    m_iRangeMax = 0;
    m_flProgress = 0.0f;
    m_Status.clear();
    m_Level.clear();
    m_bShowingSecondary = false;
    m_flSecondary = 0.0f;
    m_SecondaryText.clear();
    m_ErrorText.clear();
}

void CImGuiLoadingDialog::Open(bool bShowBackground)
{
    m_bError = false;
    m_Status.clear();
    ShowLoading();
}

void CImGuiLoadingDialog::ShowLoading()
{
    if (!IsVisible())
    {
        SetVisible(true);
        RequestFocus();
        ResetInput();
    }

    // a panel that captured the mouse and was hidden before the release (the server browser
    // on a double click) would go on getting every click, and Cancel none
    vgui2::VPANEL capture = vgui2::input()->GetMouseCapture();
    if (capture && capture != GetVPanel())
        vgui2::input()->SetMouseCapture(0);

    MoveToFront();
    vgui2::surface()->RestrictPaintToSinglePanel(GetVPanel());
}

void CImGuiLoadingDialog::CloseLoading()
{
    m_bOpen = false;
    SetVisible(false);
    vgui2::surface()->RestrictPaintToSinglePanel(0);
}

void CImGuiLoadingDialog::SetLevelName(const char* levelName)
{
    // the engine may give it as a path
    m_Level = levelName ? levelName : "";
    size_t slash = m_Level.find_last_of("/\\");
    if (slash != std::string::npos)
        m_Level.erase(0, slash + 1);
    if (m_Level.size() > 4 && !V_stricmp(m_Level.c_str() + m_Level.size() - 4, ".bsp"))
        m_Level.erase(m_Level.size() - 4);
}

bool CImGuiLoadingDialog::SetProgressPoint(int progressPoint)
{
    float range = static_cast<float>(m_iRangeMax - m_iRangeMin);
    float progress = range > 0.0f ? std::clamp(progressPoint / range, 0.0f, 1.0f) : 0.0f;

    // a redraw for every step would slow the loading down, one per percent is smooth enough
    bool moved = std::fabs(progress - m_flProgress) >= 0.01f || (progress >= 1.0f && m_flProgress < 1.0f);
    if (moved || progress < m_flProgress)
        m_flProgress = progress;
    return moved;
}

void CImGuiLoadingDialog::SetProgressRange(int min, int max)
{
    m_iRangeMin = min;
    m_iRangeMax = max;
}

void CImGuiLoadingDialog::SetStatusText(const char* statusText)
{
    m_Status = LocalizedText(statusText);
}

void CImGuiLoadingDialog::SetSecondaryProgress(float progress)
{
    // a download that is done at once isn't worth a second bar
    if (!m_bShowingSecondary && progress > 0.99f)
        return;

    if (!m_bShowingSecondary)
    {
        m_bShowingSecondary = true;
        m_flSecondaryStartTime = Now();
    }

    // a new file starts over, and the time left is counted from its start
    if (progress < m_flSecondary)
        m_flSecondaryStartTime = Now();

    if (progress != m_flSecondary)
    {
        m_flSecondary = progress;
        m_flSecondaryUpdateTime = Now();
    }
}

void CImGuiLoadingDialog::SetSecondaryProgressText(const char* statusText)
{
    m_SecondaryText = LocalizedText(statusText);
}

void CImGuiLoadingDialog::DisplayGenericError(const char* failureReason, const char* extendedReason)
{
    m_bOpen = true;
    m_bError = true;

    // the reason may be a token with a %s1 for the extended one, like the old dialog formats it
    if (extendedReason && extendedReason[0])
    {
        wchar_t format[256], extended[256], message[512];
        ToWide(failureReason, format, sizeof(format));
        ToWide(extendedReason, extended, sizeof(extended));
        g_pVGuiLocalize->ConstructString(message, sizeof(message), format, 1, extended);
        m_ErrorText = ToUTF8(message);
    }
    else
        m_ErrorText = LocalizedText(failureReason);

    ShowLoading();
}

void CImGuiLoadingDialog::Cancel()
{
    if (!m_bError)
        engine->pfnClientCmd("disconnect\n");

    CloseLoadingDialog();
}

void CImGuiLoadingDialog::DrawProgressBar(float fraction, float width, float height)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 end(pos.x + width, pos.y + height);
    drawList->AddRectFilled(pos, end, IM_COL32(255, 255, 255, 28), height * 0.5f);
    if (fraction > 0.0f)
    {
        // narrower than its height the rounded fill would bulge out of the track
        float fill = std::max(width * std::clamp(fraction, 0.0f, 1.0f), height);
        drawList->AddRectFilled(pos, ImVec2(pos.x + fill, end.y), ImGui::GetColorU32(ImGuiCol_CheckMark), height * 0.5f);
    }

    ImGui::Dummy(ImVec2(width, height));
}

bool CImGuiLoadingDialog::IsOverCancel() const
{
    int x, y;
    vgui2::input()->GetCursorPos(x, y);
    return x >= m_CancelMin.x && x < m_CancelMax.x && y >= m_CancelMin.y && y < m_CancelMax.y;
}

void CImGuiLoadingDialog::OnMousePressed(vgui2::MouseCode code)
{
    BaseClass::OnMousePressed(code);
    m_bCancelPressed = code == vgui2::MOUSE_LEFT && IsOverCancel();
}

void CImGuiLoadingDialog::OnMouseReleased(vgui2::MouseCode code)
{
    BaseClass::OnMouseReleased(code);

    bool cancel = m_bCancelPressed && code == vgui2::MOUSE_LEFT && IsOverCancel();
    m_bCancelPressed = false;
    if (cancel)
        Cancel();
}

void CImGuiLoadingDialog::OnKeyCodePressed(vgui2::KeyCode code)
{
    BaseClass::OnKeyCodePressed(code);

    if (code == vgui2::KEY_ESCAPE || (m_bError && (code == vgui2::KEY_ENTER || code == vgui2::KEY_PAD_ENTER)))
        Cancel();
}

void CImGuiLoadingDialog::PreparePreview()
{
    m_bOpen = true;
    m_Level = "de_dust2";
    m_Status = Localized("#GameUI_ParseBaseline", "Parsing game info...");
    m_flProgress = 0.45f;
}

void CImGuiLoadingDialog::Paint()
{
    // painting is restricted to this panel while loading, so the menu's background has to come from here
    if (BasePanel())
        BasePanel()->DrawMenuBackground();

    BaseClass::Paint();
}

void CImGuiLoadingDialog::DrawImGui()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 screen = viewport->Size;

    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(screen);
    ImGui::SetNextWindowFocus();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin("###Loading", nullptr, flags))
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // the picture fades to near black at the bottom, where the text goes
        ImVec2 min = viewport->Pos;
        ImVec2 max(min.x + screen.x, min.y + screen.y);
        float fadeTop = min.y + screen.y * 0.35f;
        drawList->AddRectFilled(min, ImVec2(max.x, fadeTop), IM_COL32(0, 0, 0, 70));
        drawList->AddRectFilledMultiColor(ImVec2(min.x, fadeTop), max, IM_COL32(0, 0, 0, 70), IM_COL32(0, 0, 0, 70), IM_COL32(0, 0, 0, 235), IM_COL32(0, 0, 0, 235));

        // nearly edge to edge, with just enough room that the text doesn't touch the window's border
        float margin = std::max(20.0f, std::round(screen.x * 0.02f));
        float width = screen.x - margin * 2.0f;
        ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
        const ImGuiStyle& style = ImGui::GetStyle();

        std::string title;
        std::string caption;
        if (m_bError)
            title = Localized("#GameUI_Disconnected");
        else if (!m_Level.empty())
        {
            title = m_Level;
            caption = Localized("#GameUI_Loading");
        }
        else
            title = Localized("#GameUI_Loading");

        std::string button = Localized(m_bError ? "#GameUI_Close" : "#GameUI_Cancel");
        float buttonWidth = std::max(110.0f, ImGui::CalcTextSize(button.c_str()).x + style.FramePadding.x * 4.0f);

        // the block sits on the bottom margin; its height is the last frame's, which only changes when a part comes or goes
        ImGui::SetCursorScreenPos(ImVec2(min.x + margin, max.y - margin - m_flBlockHeight));
        ImGui::BeginGroup();

        if (!caption.empty())
            ImGui::TextColored(accent, "%s", caption.c_str());

        // the title with the way out at its right end, so it stays clear of net_graph in the corner
        ImVec2 titlePos = ImGui::GetCursorScreenPos();
        ImGui::PushFont(TitleFont());
        ImGui::TextUnformatted(title.c_str());
        float titleHeight = ImGui::GetItemRectSize().y;
        ImGui::PopFont();
        ImVec2 afterTitle = ImGui::GetCursorScreenPos();

        ImGui::SetCursorScreenPos(ImVec2(titlePos.x + width - buttonWidth, titlePos.y + (titleHeight - ImGui::GetFrameHeight()) * 0.5f));
        ImGui::Button(button.c_str(), ImVec2(buttonWidth, 0));
        m_CancelMin = ImGui::GetItemRectMin();
        m_CancelMax = ImGui::GetItemRectMax();
        ImGui::SetCursorScreenPos(afterTitle);
        ImGui::Dummy(ImVec2(0, 10.0f - style.ItemSpacing.y));

        if (m_bError)
        {
            ImGui::PushFont(HeadingFont());
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width * 0.6f);
            ImGui::TextUnformatted(m_ErrorText.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopFont();
        }
        else
        {
            // the status on the left, how far along on the right, then the bar under both;
            // SameLine counts from the group's left edge
            ImGui::PushFont(HeadingFont());
            ImGui::TextUnformatted(m_Status.empty() ? " " : m_Status.c_str());
            std::string percent = std::to_string(static_cast<int>(m_flProgress * 100.0f)) + "%";
            ImGui::SameLine(width - ImGui::CalcTextSize(percent.c_str()).x);
            ImGui::TextColored(accent, "%s", percent.c_str());
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(0, 6.0f - style.ItemSpacing.y));
            DrawProgressBar(m_flProgress, width, 8.0f);

            if (m_bShowingSecondary)
            {
                ImGui::Dummy(ImVec2(0, 14.0f - style.ItemSpacing.y));
                std::string detail = std::to_string(static_cast<int>(m_flSecondary * 100.0f)) + "%";
                wchar_t remaining[256];
                if (m_flSecondary < 1.0f && vgui2::ProgressBar::ConstructTimeRemainingString(remaining, sizeof(remaining), m_flSecondaryStartTime, Now(), m_flSecondary, m_flSecondaryUpdateTime, true))
                    detail += "  ·  " + ToUTF8(remaining);

                ImGui::TextDisabled("%s", m_SecondaryText.c_str());
                ImGui::SameLine(width - ImGui::CalcTextSize(detail.c_str()).x);
                ImGui::TextDisabled("%s", detail.c_str());
                ImGui::Dummy(ImVec2(0, 4.0f - style.ItemSpacing.y));
                DrawProgressBar(m_flSecondary, width, 4.0f);
            }

            if (g_pGameUIFuncs->IsConnectedToVACSecureServer())
            {
                ImGui::Dummy(ImVec2(0, 10.0f - style.ItemSpacing.y));
                // the token has its two sentences on separate lines with a blank one between
                std::string vac = Localized("#VAC_ConnectingToSecureServer", "Connecting to a VAC secure server");
                vac.erase(std::unique(vac.begin(), vac.end(), [](char a, char b) { return a == '\n' && b == '\n'; }), vac.end());
                std::replace(vac.begin(), vac.end(), '\n', ' ');
                ImGui::TextDisabled("%s", vac.c_str());
            }
        }

        ImGui::EndGroup();
        m_flBlockHeight = ImGui::GetItemRectSize().y;

    }
    ImGui::End();

    ImGui::PopStyleVar(3);
}
