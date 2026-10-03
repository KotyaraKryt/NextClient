#pragma once

#include "ImGuiPanel.h"

#include <next_gameui/IScoreboardNext.h>
#include <vgui_controls/PHandle.h>

#include <cstdint>
#include <unordered_map>

// The scoreboard drawn with Dear ImGui instead of the client's own; client_mini feeds it through
// IScoreboardNext while +showscores is held. scoreboard_legacy 1 brings the client's one back.
// It takes no input, so the keys and the mouse stay with the game
class CImGuiScoreboard : public CImGuiPanel
{
    DECLARE_CLASS_SIMPLE(CImGuiScoreboard, CImGuiPanel);

public:
    CImGuiScoreboard();

    void SetState(const ScoreboardState& state);

protected:
    void DrawImGui() override;
    void PreparePreview() override;

private:
    void DrawTeam(ScoreboardTeam team, const char* id, float width);
    void DrawSpectators();
    // the player's Steam avatar, or a square with their initial while there is none
    void DrawAvatar(const ScoreboardPlayer& player, float size);
    // 0 until Steam has the avatar; asks it again now and then
    unsigned int AvatarTexture(uint64_t steamId);

    ScoreboardState m_State{};
    // each team's table height in the last frame, for the panel under it
    float m_flTableHeight[2] = {};

    struct Avatar
    {
        unsigned int texture = 0;
        bool requested = false;
        double nextTry = 0.0;
    };

    std::unordered_map<uint64_t, Avatar> m_Avatars;

    // the ping each player's row shows and when it was taken: bot plugins make up a new one for
    // every update, which would flicker
    struct ShownPing
    {
        int ping = 0;
        double time = 0.0;
    };

    std::unordered_map<int, ShownPing> m_ShownPings;
};

class CScoreboardNext : public IScoreboardNext
{
public:
    static void RegisterCvars();

    bool IsEnabled() override;
    void Show(const ScoreboardState& state) override;
    void Hide() override;

private:
    vgui2::DHANDLE<CImGuiScoreboard> m_hPanel;
};
