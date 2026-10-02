#pragma once

#include <steam/steam_api.h>

#include <cstdint>
#include <string>
#include <vector>

// Asks one server for its details and its players through Steam, for the server info window
class CServerInfoQuery : public ISteamMatchmakingPingResponse, public ISteamMatchmakingPlayersResponse
{
public:
    struct Player
    {
        std::string name;
        int score;
        float seconds;
    };

    // known is what the server list had, shown until the server answers
    explicit CServerInfoQuery(const gameserveritem_t& known);
    // Steam would otherwise call back into a deleted object
    ~CServerInfoQuery();

    CServerInfoQuery(const CServerInfoQuery&) = delete;
    CServerInfoQuery& operator=(const CServerInfoQuery&) = delete;

    // asks for the details again, and for the players once the details come
    void Refresh();

    [[nodiscard]] const gameserveritem_t& GetServer() const { return m_Server; }
    [[nodiscard]] const std::vector<Player>& GetPlayers() const { return m_Players; }
    [[nodiscard]] bool IsBusy() const { return m_hPingQuery != 0 || m_hPlayersQuery != 0; }
    [[nodiscard]] bool IsNotResponding() const { return m_bNotResponding; }
    // goes up with every answer to Refresh, so the window can tell a new one from the last
    [[nodiscard]] uint32_t GetResponseCount() const { return m_iResponses; }

private:
    // ISteamMatchmakingPingResponse
    void ServerResponded(gameserveritem_t& server) override;
    void ServerFailedToRespond() override;

    // ISteamMatchmakingPlayersResponse
    void AddPlayerToList(const char* name, int score, float seconds) override;
    void PlayersFailedToRespond() override;
    void PlayersRefreshComplete() override;

    void CancelQueries();

    gameserveritem_t m_Server;
    uint32_t m_iIP;
    uint16_t m_iQueryPort;
    HServerQuery m_hPingQuery = 0;
    HServerQuery m_hPlayersQuery = 0;

    // the players of the last full answer, and of the one coming in
    std::vector<Player> m_Players;
    std::vector<Player> m_IncomingPlayers;
    bool m_bNotResponding = false;
    uint32_t m_iResponses = 0;
};
