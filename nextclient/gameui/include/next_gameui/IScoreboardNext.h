#pragma once

#include "tier1/interface.h"

#include <cstdint>

// What client_mini knows about the players and hands the ImGui scoreboard every frame it's up
enum class ScoreboardTeam
{
    Unassigned = 0,
    Terrorist = 1,
    CounterTerrorist = 2,
    Spectator = 3,
};

struct ScoreboardPlayer
{
    int index;
    char name[64];
    // 0 for bots and players without a Steam account the server knows of
    uint64_t steam_id;
    ScoreboardTeam team;
    int frags;
    int deaths;
    int ping;
    bool dead;
    bool bomb;
    bool vip;
    bool bot;
    bool self;
};

struct ScoreboardState
{
    // rounds won, from the TeamScore messages
    int terrorist_score;
    int ct_score;

    int player_count;
    ScoreboardPlayer players[32];
};

class IScoreboardNext : public IBaseInterface
{
public:
    // false while scoreboard_legacy asks for the client's own scoreboard
    virtual bool IsEnabled() = 0;
    // shows the scoreboard with this content, or updates it if it's up
    virtual void Show(const ScoreboardState& state) = 0;
    virtual void Hide() = 0;
};

#define SCOREBOARD_NEXT_INTERFACE_VERSION "ScoreboardNext002"
