#include "ImGuiDemoPlayer.h"
#include "GameUi.h"
#include "ImGuiForm.h"

#include <vgui/IInputInternal.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Controls.h>
#include <FileSystem.h>

// the HLTV headers need the SDK's types without including them, like DemoPlayerDialog.cpp
#include <basetypes.h>
#include <mathlib/mathlib.h>
#include <common.h>
#include <cvardef.h>
#include <pm_defs.h>
#include <kbutton.h>
#include <r_efx.h>
#include <r_studioint.h>
#include <custom.h>
#include <IBaseSystem.h>
#include <common/BitBuffer.h>
#include <IDemoPlayer.h>
#include <IWorld.h>
#include <IEngineWrapper.h>

#include <tier1/strtools.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace ImGuiForm;

namespace
{
    cvar_t* g_pLegacyCvar = nullptr;
    cvar_t* g_pAutoHideCvar = nullptr;
    cvar_t* g_pCompactCvar = nullptr;

    constexpr float kSeekStep = 10.0f;
    constexpr double kAutoHideDelay = 2.5;

    // the speeds the slower and faster buttons step through
    const float kSpeeds[] = { 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 10.0f };
    // the demo player module goes no faster than this; above it the rest is made up by moving its clock
    constexpr float kModuleMaxSpeed = 4.0f;

    enum class Icon
    {
        Start,
        StepBack,
        Play,
        Pause,
        StepForward,
        End,
        Slower,
        Faster,
        Stop,
        Expand,
        Shrink,
    };

    // the transport icons are drawn rather than taken from a font, which has them in odd sizes or not at all
    void DrawIcon(ImDrawList* drawList, ImVec2 center, float size, Icon icon, ImU32 color)
    {
        float h = size * 0.5f;
        float bar = std::max(2.0f, std::round(size * 0.16f));

        auto triangleRight = [&](float left, float width)
        {
            drawList->AddTriangleFilled(ImVec2(left, center.y - h), ImVec2(left + width, center.y), ImVec2(left, center.y + h), color);
        };
        auto triangleLeft = [&](float right, float width)
        {
            drawList->AddTriangleFilled(ImVec2(right, center.y - h), ImVec2(right, center.y + h), ImVec2(right - width, center.y), color);
        };
        auto bar_at = [&](float left)
        {
            drawList->AddRectFilled(ImVec2(left, center.y - h), ImVec2(left + bar, center.y + h), color);
        };

        float left = center.x - h;
        float right = center.x + h;
        switch (icon)
        {
            case Icon::Play:
                triangleRight(left + size * 0.1f, size * 0.85f);
                break;
            case Icon::Pause:
                drawList->AddRectFilled(ImVec2(left + size * 0.15f, center.y - h), ImVec2(left + size * 0.4f, center.y + h), color);
                drawList->AddRectFilled(ImVec2(right - size * 0.4f, center.y - h), ImVec2(right - size * 0.15f, center.y + h), color);
                break;
            case Icon::Stop:
                drawList->AddRectFilled(ImVec2(left + size * 0.1f, center.y - h * 0.8f), ImVec2(right - size * 0.1f, center.y + h * 0.8f), color, 1.0f);
                break;
            case Icon::StepForward:
                triangleRight(left, size * 0.7f);
                bar_at(right - bar);
                break;
            case Icon::StepBack:
                bar_at(left);
                triangleLeft(right, size * 0.7f);
                break;
            case Icon::Start:
                bar_at(left);
                triangleLeft(center.x + size * 0.1f, size * 0.45f);
                triangleLeft(right, size * 0.45f);
                break;
            case Icon::End:
                triangleRight(left, size * 0.45f);
                triangleRight(center.x - size * 0.1f, size * 0.45f);
                bar_at(right - bar);
                break;
            case Icon::Slower:
                triangleLeft(center.x, size * 0.5f);
                triangleLeft(right, size * 0.5f);
                break;
            case Icon::Faster:
                triangleRight(left, size * 0.5f);
                triangleRight(center.x, size * 0.5f);
                break;
            case Icon::Expand:
                drawList->AddRect(ImVec2(left, center.y - h * 0.8f), ImVec2(right, center.y + h * 0.8f), color, 1.0f, 0, bar * 0.8f);
                break;
            case Icon::Shrink:
                drawList->AddRectFilled(ImVec2(left, center.y + h * 0.4f), ImVec2(right, center.y + h * 0.4f + bar), color);
                break;
        }
    }

    bool IconButton(const char* id, Icon icon, const char* tooltipToken, const char* tooltipEnglish, bool accent = false)
    {
        float side = ImGui::GetFrameHeight() + 6.0f;
        if (accent)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(ImGuiCol_CheckMark, 0.85f));
        }
        bool pressed = ImGui::Button(id, ImVec2(side, side));
        if (accent)
            ImGui::PopStyleColor(2);

        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        DrawIcon(ImGui::GetWindowDrawList(), ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f), side * 0.42f, icon, ImGui::GetColorU32(ImGuiCol_Text));

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", CImGuiPanel::Localized(tooltipToken, tooltipEnglish).c_str());
        return pressed;
    }

    std::string FormatTime(float seconds)
    {
        seconds = std::max(seconds, 0.0f);
        int total = static_cast<int>(seconds);
        char text[32];
        snprintf(text, sizeof(text), "%02d:%02d.%02d", total / 60, total % 60, static_cast<int>(seconds * 100.0f) % 100);
        return text;
    }

    std::string ToLower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)tolower(c); });
        return text;
    }
}

CImGuiDemoPlayer::CImGuiDemoPlayer() : BaseClass("demoplayer_layout.ini")
{
    SetVisible(false);
}

void CImGuiDemoPlayer::RegisterCvars()
{
    g_pLegacyCvar = engine->pfnRegisterVariable("demoui_legacy", "0", FCVAR_ARCHIVE);
    g_pAutoHideCvar = engine->pfnRegisterVariable("demoui_autohide", "1", FCVAR_ARCHIVE);
    g_pCompactCvar = engine->pfnRegisterVariable("demoui_compact", "0", FCVAR_ARCHIVE);
}

bool CImGuiDemoPlayer::UseLegacyDialog()
{
    return g_pLegacyCvar && g_pLegacyCvar->value != 0.0f;
}

bool CImGuiDemoPlayer::LoadModules()
{
    if (m_pDemoPlayer && m_pWorld && m_pEngine)
        return true;

    IBaseSystem* system = SystemWrapper();
    if (!system)
        return false;

    m_pEngine = reinterpret_cast<IEngineWrapper*>(system->GetModule("enginewrapper002", "", nullptr));
    // the module system hands out ISystemModule pointers, which the HLTV interfaces aren't declared to derive from
    m_pDemoPlayer = reinterpret_cast<IDemoPlayer*>(system->GetModule(DEMOPLAYER_INTERFACE_VERSION, "", nullptr));
    m_pWorld = m_pDemoPlayer ? m_pDemoPlayer->GetWorld() : nullptr;

    if (!m_pEngine || !m_pDemoPlayer || !m_pWorld)
    {
        system->Printf("CImGuiDemoPlayer: couldn't get the engine wrapper, demo player or world module.\n");
        return false;
    }
    return true;
}

void CImGuiDemoPlayer::Activate()
{
    if (!LoadModules())
        return;

    SetVisible(true);
    MoveToFront();
    RequestFocus();
    ResetInput();
    m_bFocusWindow = true;
}

void CImGuiDemoPlayer::Close()
{
    SetVisible(false);
}

void CImGuiDemoPlayer::DemoSelected(const char* demoName)
{
    if (!LoadModules())
        return;

    char command[300];
    V_snprintf(command, sizeof(command), "viewdemo \"%s\"\n", demoName);

    m_pDemoPlayer->Stop();
    m_flSpeed = 1.0f;
    m_pEngine->Cbuf_AddText(command);
}

bool CImGuiDemoPlayer::GetTimeRange(float& start, float& end)
{
    // the player knows the whole demo; the world only holds the frames read so far
    start = static_cast<float>(m_pDemoPlayer->GetStartTime());
    end = static_cast<float>(m_pDemoPlayer->GetEndTime());
    if (end > start)
        return true;

    frame_t* first = m_pWorld->GetFirstFrame();
    frame_t* last = m_pWorld->GetLastFrame();
    if (!first || !last)
        return false;

    // frames more than 2 seconds apart at the beginning were recorded before the demo really started
    frame_t* previous = nullptr;
    for (frame_t* frame = first; frame; frame = m_pWorld->GetFrameBySeqNr(frame->seqnr + 1))
    {
        if (previous && frame->time - previous->time > 2.0f)
        {
            first = frame;
            break;
        }
        previous = frame;
    }

    start = first->time;
    end = last->time;
    return end > start;
}

void CImGuiDemoPlayer::Play()
{
    m_pDemoPlayer->SetPaused(false);
}

void CImGuiDemoPlayer::Seek(float seconds)
{
    float start, end;
    if (!GetTimeRange(start, end))
        return;

    float time = std::clamp(static_cast<float>(m_pDemoPlayer->GetWorldTime()) + seconds, start, end);
    m_pDemoPlayer->SetWorldTime(time, false);
}

void CImGuiDemoPlayer::Pause()
{
    m_pDemoPlayer->SetPaused(true);
    m_pEngine->Cbuf_AddText("stopsound\n");
}

void CImGuiDemoPlayer::Step(int direction)
{
    frame_t* frame = m_pWorld->GetFrameByTime(m_pDemoPlayer->GetWorldTime());
    if (!frame)
        return;

    frame = m_pWorld->GetFrameBySeqNr(frame->seqnr + direction);
    if (!frame)
        return;

    m_pDemoPlayer->SetWorldTime(frame->time, false);
    m_pDemoPlayer->SetPaused(true);
}

void CImGuiDemoPlayer::GoToStart()
{
    frame_t* first = m_pWorld->GetFirstFrame();
    if (!first)
        return;

    m_pDemoPlayer->SetWorldTime(first->time - 0.01f, false);
    m_flSpeed = 1.0f;
    m_pDemoPlayer->SetTimeScale(1.0f);
    m_pDemoPlayer->SetPaused(true);
}

void CImGuiDemoPlayer::GoToEnd()
{
    frame_t* last = m_pWorld->GetLastFrame();
    if (!last)
        return;

    m_pDemoPlayer->SetWorldTime(last->time, false);
    Pause();
}

void CImGuiDemoPlayer::ChangeSpeed(bool faster)
{
    // the step nearest to the current speed, then one either way
    int nearest = 0;
    for (int i = 1; i < (int)std::size(kSpeeds); i++)
    {
        if (std::fabs(kSpeeds[i] - m_flSpeed) < std::fabs(kSpeeds[nearest] - m_flSpeed))
            nearest = i;
    }

    int next = std::clamp(nearest + (faster ? 1 : -1), 0, (int)std::size(kSpeeds) - 1);
    m_flSpeed = kSpeeds[next];
    m_pDemoPlayer->SetTimeScale(std::min(m_flSpeed, kModuleMaxSpeed));
}

void CImGuiDemoPlayer::AdvanceBeyondModuleSpeed()
{
    double now = vgui2::system()->GetCurrentTime();
    float dt = m_flLastAdvance > 0.0 ? static_cast<float>(std::min(now - m_flLastAdvance, 0.1)) : 0.0f;
    m_flLastAdvance = now;

    // someone else (the console, the old dialog) changed the speed; follow it
    float scale = m_pDemoPlayer->GetTimeScale();
    if (std::fabs(scale - std::min(m_flSpeed, kModuleMaxSpeed)) > 0.01f)
        m_flSpeed = scale;

    if (m_flSpeed <= kModuleMaxSpeed || !m_pDemoPlayer->IsActive() || m_pDemoPlayer->IsPaused())
        return;

    float start, end;
    if (!GetTimeRange(start, end))
        return;

    double time = m_pDemoPlayer->GetWorldTime() + (m_flSpeed - kModuleMaxSpeed) * dt;
    m_pDemoPlayer->SetWorldTime(std::min<double>(time, end), false);
}

std::string CImGuiDemoPlayer::SpeedText() const
{
    char speed[16];
    if (m_flSpeed < 0.9f)
        snprintf(speed, sizeof(speed), "x1/%d", static_cast<int>(std::round(1.0f / m_flSpeed)));
    else
        snprintf(speed, sizeof(speed), "x%d", static_cast<int>(std::round(m_flSpeed)));
    return speed;
}

void CImGuiDemoPlayer::Stop()
{
    m_pDemoPlayer->Stop();
    m_pEngine->Cbuf_AddText("stopdemo\n");
}

void CImGuiDemoPlayer::LoadDemoList()
{
    m_Demos.clear();
    FileFindHandle_t handle = 0;
    for (const char* file = g_pFullFileSystem->FindFirst("*.dem", &handle, "GAME"); file; file = g_pFullFileSystem->FindNext(handle))
        m_Demos.push_back(file);
    g_pFullFileSystem->FindClose(handle);

    std::sort(m_Demos.begin(), m_Demos.end(), [](const std::string& a, const std::string& b) { return V_stricmp(a.c_str(), b.c_str()) < 0; });
    m_Demos.erase(std::unique(m_Demos.begin(), m_Demos.end()), m_Demos.end());
}

namespace
{
    float SideOf() { return ImGui::GetFrameHeight() + 6.0f; }
    float SeekWidth() { return SideOf() * 1.3f; }

    float ButtonWidth(const std::string& text)
    {
        return ImGui::CalcTextSize(text.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    }

    float CheckboxWidth(const std::string& text)
    {
        return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(text.c_str()).x;
    }
}

float CImGuiDemoPlayer::TransportWidth() const
{
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    // start, step back, play, step forward, end and the two seeks, then a gap and the two speeds
    return SideOf() * 5.0f + SeekWidth() * 2.0f + spacing * 6.0f + spacing * 3.0f + SideOf() * 2.0f + spacing;
}

float CImGuiDemoPlayer::ContentWidth() const
{
    const ImGuiStyle& style = ImGui::GetStyle();

    float bottom = CheckboxWidth(Localized("#GameUI_DemoAutoHide", "Hide while playing")) + style.ItemSpacing.x + SideOf() + style.ItemSpacing.x * 3.0f
        + SideOf() + style.ItemSpacing.x + ButtonWidth(Localized("#GameUI_DemoOpen", "Open")) + style.FramePadding.x * 2.0f;

    return std::max({ 520.0f, TransportWidth(), bottom });
}

void CImGuiDemoPlayer::DrawImGui()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->GetCenter().x, viewport->Pos.y + viewport->Size.y - 40.0f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 1.0f));
    bool compact = g_pCompactCvar && g_pCompactCvar->value != 0.0f;
    // the window is as wide as its widest row, so nothing pushed to the right edge runs into the rest
    if (!compact)
        ImGui::SetNextWindowContentSize(ImVec2(ContentWidth(), 0.0f));

    if (m_bFocusWindow)
    {
        ImGui::SetNextWindowFocus();
        m_bFocusWindow = false;
    }

    UpdateAutoHide();
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_flAlpha);

    bool open = true;
    const char* fileName = m_pDemoPlayer && m_pDemoPlayer->IsActive() ? m_pDemoPlayer->GetFileName() : nullptr;
    std::string title = Localized("#GameUI_DemoPlayer", "Demo Player");
    if (fileName && fileName[0])
        title += std::string(" - ") + fileName;
    title += "###DemoPlayer";

    // switching between the full and the compact player keeps the window's bottom middle where it was,
    // so it grows up and to the sides instead of off the bottom of the screen; a resize takes a frame
    // or two to settle, so the anchor holds for a few
    if (compact != m_bWasCompact && m_bHaveAnchor)
        m_iAnchorFrames = 3;
    m_bWasCompact = compact;
    if (m_iAnchorFrames > 0)
    {
        ImGui::SetNextWindowPos(m_BottomCenter, ImGuiCond_Always, ImVec2(0.5f, 1.0f));
        m_iAnchorFrames--;
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
    if (compact)
        flags |= ImGuiWindowFlags_NoTitleBar;
    if (ImGui::Begin(title.c_str(), &open, flags))
    {
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        m_BottomCenter = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y);
        m_bHaveAnchor = true;

        if (LoadModules())
        {
            AdvanceBeyondModuleSpeed();
            if (compact)
                DrawCompact();
            else
            {
                DrawTransport();
                DrawBottomRow();
            }
        }

        DrawLoadPopup();
    }
    ImGui::End();

    ImGui::PopStyleVar();

    if (!open)
        Close();
}

void CImGuiDemoPlayer::UpdateAutoHide()
{
    double now = vgui2::system()->GetCurrentTime();
    float dt = m_flLastFrame > 0.0 ? static_cast<float>(now - m_flLastFrame) : 0.0f;
    m_flLastFrame = now;

    // the panel only covers the window, so its own cursor moves stop at the edge; ask VGUI instead
    int x, y;
    vgui2::input()->GetCursorPos(x, y);
    const ImGuiIO& io = ImGui::GetIO();
    bool anyButton = io.MouseDown[0] || io.MouseDown[1] || io.MouseDown[2];
    if (x != m_iLastCursorX || y != m_iLastCursorY || anyButton || io.InputQueueCharacters.Size > 0 || m_flLastActivity == 0.0)
    {
        m_iLastCursorX = x;
        m_iLastCursorY = y;
        m_flLastActivity = now;
    }

    bool playing = m_pDemoPlayer && m_pDemoPlayer->IsActive() && !m_pDemoPlayer->IsPaused();
    bool hide = g_pAutoHideCvar && g_pAutoHideCvar->value != 0.0f && playing
        && now - m_flLastActivity > kAutoHideDelay && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);

    // a quick fade in, a slower fade out
    float target = hide ? 0.0f : 1.0f;
    float speed = hide ? 2.0f : 8.0f;
    m_flAlpha = target > m_flAlpha ? std::min(target, m_flAlpha + dt * speed) : std::max(target, m_flAlpha - dt * speed);
}

void CImGuiDemoPlayer::DrawBottomRow()
{
    const ImGuiStyle& style = ImGui::GetStyle();
    bool active = m_pDemoPlayer->IsActive();

    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Separator();

    if (g_pAutoHideCvar)
    {
        bool autoHide = g_pAutoHideCvar->value != 0.0f;
        ImGui::AlignTextToFramePadding();
        if (ImGui::Checkbox(Localized("#GameUI_DemoAutoHide", "Hide while playing").c_str(), &autoHide))
            engine->Cvar_SetValue("demoui_autohide", autoHide ? 1.0f : 0.0f);
        ImGui::SameLine();
    }

    if (IconButton("##Compact", Icon::Shrink, "#GameUI_DemoCompact", "Compact player: just the bar"))
        engine->Cvar_SetValue("demoui_compact", 1.0f);
    ImGui::SameLine();

    // stopping and loading another at the right end
    std::string open = Localized("#GameUI_DemoOpen", "Open");
    float side = SideOf();
    float openWidth = ButtonWidth(open) + style.FramePadding.x * 2.0f;
    ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - openWidth - side - style.ItemSpacing.x);
    ImGui::BeginDisabled(!active);
    if (IconButton("##Stop", Icon::Stop, "#GameUI_DemoStop", "Stop"))
        Stop();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(open.c_str(), ImVec2(openWidth, side)))
        m_bOpenLoadPopup = true;
}

void CImGuiDemoPlayer::DrawTimeSlider(float width)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    bool active = m_pDemoPlayer->IsActive();
    float start = 0.0f, end = 0.0f;
    bool haveRange = active && GetTimeRange(start, end);
    float now = static_cast<float>(m_pDemoPlayer->GetWorldTime());

    // the time bar: dragging it scrubs through the demo, paused like the old dialog did
    ImGui::BeginDisabled(!haveRange);
    float time = haveRange ? std::clamp(now, start, end) : 0.0f;
    ImGui::SetNextItemWidth(width);
    if (ImGui::SliderFloat("##Time", &time, start, haveRange ? end : 1.0f, "", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput) && haveRange)
    {
        m_pDemoPlayer->SetWorldTime(time, false);
        if (!m_pDemoPlayer->IsPaused())
            Pause();
    }
    // the time under the cursor, where a click would go
    if (haveRange && ImGui::IsItemHovered() && !ImGui::IsItemActive())
    {
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        float grab = style.GrabMinSize * 0.5f + style.FramePadding.x;
        float t = std::clamp((ImGui::GetIO().MousePos.x - min.x - grab) / std::max(1.0f, max.x - min.x - grab * 2.0f), 0.0f, 1.0f);
        ImGui::SetTooltip("%s", FormatTime(t * (end - start)).c_str());
    }
    ImGui::EndDisabled();
}

void CImGuiDemoPlayer::HandleKeys()
{
    // the keys a video player has, while nothing is being typed
    if (!m_pDemoPlayer->IsActive() || !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || ImGui::GetIO().WantTextInput
        || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
        m_pDemoPlayer->IsPaused() ? Play() : Pause();
    bool shift = ImGui::GetIO().KeyShift;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
        shift ? Seek(-kSeekStep) : Step(-1);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        shift ? Seek(kSeekStep) : Step(1);
}

void CImGuiDemoPlayer::DrawCompact()
{
    // one row: play or pause, the time bar, where it is and how fast, and the way back to the full window
    const ImGuiStyle& style = ImGui::GetStyle();
    bool active = m_pDemoPlayer->IsActive();
    float start = 0.0f, end = 0.0f;
    bool haveRange = active && GetTimeRange(start, end);
    float now = static_cast<float>(m_pDemoPlayer->GetWorldTime());

    ImGui::BeginDisabled(!active);
    bool paused = m_pDemoPlayer->IsPaused();
    if (IconButton("##PlayPause", paused ? Icon::Play : Icon::Pause, paused ? "#GameUI_DemoPlay" : "#GameUI_DemoPause", paused ? "Play" : "Pause", true))
        paused ? Play() : Pause();
    ImGui::EndDisabled();

    ImGui::SameLine();
    float side = SideOf();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (side - ImGui::GetFrameHeight()) * 0.5f);
    DrawTimeSlider(300.0f);

    ImGui::SameLine();
    std::string position = haveRange ? FormatTime(now - start) + " / " + FormatTime(end - start) : "--:--.-- / --:--.--";
    ImGui::TextUnformatted(position.c_str());
    ImGui::SameLine();
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", SpeedText().c_str());

    ImGui::SameLine(0.0f, style.ItemSpacing.x * 2.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (side - ImGui::GetFrameHeight()) * 0.5f);
    if (IconButton("##Expand", Icon::Expand, "#GameUI_DemoFull", "Full player"))
        engine->Cvar_SetValue("demoui_compact", 0.0f);

    HandleKeys();
}

void CImGuiDemoPlayer::DrawTransport()
{
    const ImGuiStyle& style = ImGui::GetStyle();
    bool active = m_pDemoPlayer->IsActive();

    float start = 0.0f, end = 0.0f;
    bool haveRange = active && GetTimeRange(start, end);
    float now = static_cast<float>(m_pDemoPlayer->GetWorldTime());

    DrawTimeSlider(ImGui::GetContentRegionAvail().x);

    // where it is, how long it is, how fast and whether it plays
    std::string position = haveRange ? FormatTime(now - start) + " / " + FormatTime(end - start) : "--:--.-- / --:--.--";
    std::string speed = SpeedText();

    std::string state;
    if (!active)
        state = Localized("#GameUI_DemoStopped", "Stopped");
    else if (m_pDemoPlayer->IsPaused())
        state = Localized("#GameUI_DemoPaused", "Paused");
    else if (!haveRange && m_pDemoPlayer->IsLoading())
        state = Localized("#GameUI_Loading");
    else
        state = Localized("#GameUI_DemoPlaying", "Playing");

    ImGui::TextUnformatted(position.c_str());
    ImGui::SameLine();
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", speed.c_str());
    float stateWidth = ImGui::CalcTextSize(state.c_str()).x;
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - stateWidth);
    ImGui::TextDisabled("%s", state.c_str());

    ImGui::Dummy(ImVec2(0, 2));

    // the transport row in the middle of the window
    float side = SideOf();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (ImGui::GetContentRegionAvail().x - TransportWidth()) * 0.5f));
    ImGui::BeginDisabled(!active);
    if (IconButton("##Start", Icon::Start, "#GameUI_DemoStart", "To the start"))
        GoToStart();
    ImGui::SameLine();
    if (ImGui::Button("-10", ImVec2(SeekWidth(), side)))
        Seek(-kSeekStep);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", Localized("#GameUI_DemoBack10", "10 seconds back (Shift+Left)").c_str());
    ImGui::SameLine();
    if (IconButton("##StepBack", Icon::StepBack, "#GameUI_DemoStepBack", "Frame back"))
        Step(-1);
    ImGui::SameLine();
    bool paused = m_pDemoPlayer->IsPaused();
    if (IconButton("##PlayPause", paused ? Icon::Play : Icon::Pause, paused ? "#GameUI_DemoPlay" : "#GameUI_DemoPause", paused ? "Play" : "Pause", true))
        paused ? Play() : Pause();
    ImGui::SameLine();
    if (IconButton("##StepForward", Icon::StepForward, "#GameUI_DemoStepForward", "Frame forward"))
        Step(1);
    ImGui::SameLine();
    if (ImGui::Button("+10", ImVec2(SeekWidth(), side)))
        Seek(kSeekStep);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", Localized("#GameUI_DemoForward10", "10 seconds forward (Shift+Right)").c_str());
    ImGui::SameLine();
    if (IconButton("##End", Icon::End, "#GameUI_DemoEnd", "To the end"))
        GoToEnd();

    ImGui::SameLine(0.0f, style.ItemSpacing.x * 3.0f);
    if (IconButton("##Slower", Icon::Slower, "#GameUI_DemoSlower", "Slower"))
        ChangeSpeed(false);
    ImGui::SameLine();
    if (IconButton("##Faster", Icon::Faster, "#GameUI_DemoFaster", "Faster"))
        ChangeSpeed(true);
    ImGui::EndDisabled();

    HandleKeys();
}

void CImGuiDemoPlayer::DrawLoadPopup()
{
    if (m_bOpenLoadPopup)
    {
        m_bOpenLoadPopup = false;
        LoadDemoList();
        m_szSearch[0] = '\0';
        if (m_SelectedDemo.empty() && !m_Demos.empty())
            m_SelectedDemo = m_Demos.front();
        ImGui::OpenPopup("###LoadDemo");
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 460), ImGuiCond_Appearing);

    bool open = true;
    std::string title = Localized("#GameUI_LoadDemo", "Load demo") + "###LoadDemo";
    if (!ImGui::BeginPopupModal(title.c_str(), &open, ImGuiWindowFlags_NoCollapse))
        return;

    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    ImGui::InputTextWithHint("##Search", Localized("#GameUI_DemoSearch", "Search demos").c_str(), m_szSearch, sizeof(m_szSearch));

    std::string search = ToLower(m_szSearch);
    std::string load;
    float footer = ImGui::GetFrameHeight() + style.ItemSpacing.y;

    ImGui::BeginChild("Demos", ImVec2(0, -footer), true);
    for (const std::string& demo : m_Demos)
    {
        if (!search.empty() && ToLower(demo).find(search) == std::string::npos)
            continue;

        if (ImGui::Selectable(demo.c_str(), demo == m_SelectedDemo, ImGuiSelectableFlags_AllowDoubleClick))
        {
            m_SelectedDemo = demo;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                load = demo;
        }
    }
    if (m_Demos.empty())
        ImGui::TextDisabled("%s", Localized("#GameUI_DemoNoDemos", "No demos in the game folder").c_str());
    ImGui::EndChild();

    std::string loadText = Localized("#GameUI_Load", "Load");
    std::string cancelText = Localized("#GameUI_Cancel");
    float buttonWidth = 96.0f;
    for (const std::string* text : { &loadText, &cancelText })
        buttonWidth = std::max(buttonWidth, ImGui::CalcTextSize(text->c_str()).x + style.FramePadding.x * 2.0f);

    ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - buttonWidth * 2.0f - style.ItemSpacing.x);
    if (ImGui::Button(cancelText.c_str(), ImVec2(buttonWidth, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        open = false;
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(ImGuiCol_CheckMark, 0.85f));
    ImGui::BeginDisabled(m_SelectedDemo.empty());
    if (ImGui::Button(loadText.c_str(), ImVec2(buttonWidth, 0)) || (ImGui::IsKeyPressed(ImGuiKey_Enter) && !m_SelectedDemo.empty()))
        load = m_SelectedDemo;
    ImGui::EndDisabled();
    ImGui::PopStyleColor(2);

    if (!load.empty())
    {
        DemoSelected(load.c_str());
        open = false;
    }

    if (!open)
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
