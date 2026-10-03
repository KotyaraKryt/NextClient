#include "scoreboard.h"
#include "main.h"
#include "hlsdk.h"
#include "parsemsg.h"

#include <next_gameui/IScoreboardNext.h>

#include <cstdlib>
#include <cstring>

namespace
{
    IScoreboardNext* g_Scoreboard = nullptr;
    // the client was asked to show its scoreboard and ours is up instead
    bool g_bVisible = false;
    // the scoreboard key is down
    bool g_bHeld = false;
    int g_TerroristScore = 0;
    int g_CTScore = 0;

    ScoreboardTeam TeamFromName(const char* name)
    {
        if (!strcmp(name, "CT"))
            return ScoreboardTeam::CounterTerrorist;
        if (!strcmp(name, "TERRORIST"))
            return ScoreboardTeam::Terrorist;
        if (!strcmp(name, "SPECTATOR"))
            return ScoreboardTeam::Spectator;
        return ScoreboardTeam::Unassigned;
    }

    void FillState(ScoreboardState& state)
    {
        memset(&state, 0, sizeof(state));
        state.terrorist_score = g_TerroristScore;
        state.ct_score = g_CTScore;

        extra_player_info_t* extra = client()->g_PlayerExtraInfo;
        int maxClients = gEngfuncs.GetMaxClients();
        for (int i = 1; i <= maxClients && state.player_count < (int)std::size(state.players); i++)
        {
            hud_player_info_t info{};
            gEngfuncs.pfnGetPlayerInfo(i, &info);
            if (!info.name || !info.name[0])
                continue;

            ScoreboardPlayer& player = state.players[state.player_count++];
            player.index = i;
            V_strncpy(player.name, info.name, sizeof(player.name));
            // the server sends pings only while the scoreboard key is held, which it is now
            player.ping = info.ping;
            player.self = info.thisplayer != 0;

            const char* bot = gEngfuncs.PlayerInfo_ValueForKey(i, "*bot");
            player.bot = bot && !strcmp(bot, "1");

            // for the avatar; the engine fills m_nSteamID, *sid is the userinfo key servers set
            player.steam_id = info.m_nSteamID;
            if (!player.steam_id && !player.bot)
            {
                if (const char* sid = gEngfuncs.PlayerInfo_ValueForKey(i, "*sid"))
                    player.steam_id = strtoull(sid, nullptr, 10);
            }

            if (extra)
            {
                player.team = TeamFromName(extra[i].teamname);
                player.frags = extra[i].frags;
                player.deaths = extra[i].deaths;
                player.dead = extra[i].dead;
                player.bomb = extra[i].has_c4;
                player.vip = extra[i].vip;
            }
        }
    }

    bool UseOurs()
    {
        return g_Scoreboard && g_Scoreboard->IsEnabled();
    }
}

void ScoreboardSubscribe(nitroapi::ClientData* client_data, std::vector<std::shared_ptr<nitroapi::Unsubscriber>>& unsub)
{
#ifndef _WIN32
    unsub.emplace_back(client_data->TeamFortressViewport__ShowScoreBoard |= [](void* viewport, const auto& next) {
        if (!UseOurs())
        {
            next->Invoke(viewport);
            return;
        }

        g_bVisible = true;
    });

    // the key itself: the client's own checks for hiding its scoreboard go wrong while ours is up,
    // and while dead they don't come at all
    unsub.emplace_back(client_data->IN_ScoreDown += [] {
        g_bHeld = true;
        if (UseOurs())
            g_bVisible = true;
    });

    unsub.emplace_back(client_data->IN_ScoreUp += [] {
        g_bHeld = false;
    });

    unsub.emplace_back(client_data->TeamFortressViewport__HideScoreBoard += [](void* viewport) {
        g_bVisible = false;
        if (g_Scoreboard)
            g_Scoreboard->Hide();
    });

    // the rounds each team has won: the team's name, then the score
    unsub.emplace_back(client_data->UserMsg_TeamScore += [](const char* name, int size, void* data, int result) {
        BEGIN_READ(data, size);
        char team[32];
        V_strncpy(team, READ_STRING(), sizeof(team));
        int score = READ_SHORT();

        switch (TeamFromName(team))
        {
            case ScoreboardTeam::Terrorist: g_TerroristScore = score; break;
            case ScoreboardTeam::CounterTerrorist: g_CTScore = score; break;
            default: break;
        }
    });
#endif
}

void ScoreboardInit(CreateInterfaceFn gameui_factory)
{
    g_Scoreboard = static_cast<IScoreboardNext*>(gameui_factory(SCOREBOARD_NEXT_INTERFACE_VERSION, nullptr));
}

void ScoreboardFrame(int intermission)
{
    if (!g_bVisible || !g_Scoreboard)
        return;

    // ours goes when the key is let go, except at the end of the map where it stays
    if (!g_bHeld && !intermission)
    {
        g_bVisible = false;
        g_Scoreboard->Hide();
        return;
    }

    // scoreboard_legacy turned on while ours was up
    if (!g_Scoreboard->IsEnabled())
    {
        g_bVisible = false;
        g_Scoreboard->Hide();
        return;
    }

    static ScoreboardState state;
    FillState(state);
    g_Scoreboard->Show(state);
}

void ScoreboardReset()
{
    g_TerroristScore = 0;
    g_CTScore = 0;
}

void ScoreboardShutdown()
{
    if (g_Scoreboard)
        g_Scoreboard->Hide();
    g_Scoreboard = nullptr;
    g_bVisible = false;
}
