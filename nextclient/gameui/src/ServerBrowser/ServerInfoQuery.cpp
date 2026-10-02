#include "ServerInfoQuery.h"

#include <utility>

CServerInfoQuery::CServerInfoQuery(const gameserveritem_t& known) :
    m_Server(known),
    m_iIP(known.m_NetAdr.GetIP()),
    m_iQueryPort(known.m_NetAdr.GetQueryPort())
{
    Refresh();
}

CServerInfoQuery::~CServerInfoQuery()
{
    CancelQueries();
}

void CServerInfoQuery::Refresh()
{
    if (m_hPingQuery)
        return;

    m_hPingQuery = SteamMatchmakingServers()->PingServer(m_iIP, m_iQueryPort, this);
}

void CServerInfoQuery::CancelQueries()
{
    if (m_hPingQuery)
        SteamMatchmakingServers()->CancelServerQuery(m_hPingQuery);
    if (m_hPlayersQuery)
        SteamMatchmakingServers()->CancelServerQuery(m_hPlayersQuery);

    m_hPingQuery = 0;
    m_hPlayersQuery = 0;
}

void CServerInfoQuery::ServerResponded(gameserveritem_t& server)
{
    m_hPingQuery = 0;
    m_Server = server;
    m_bNotResponding = false;
    m_iResponses++;

    if (!m_hPlayersQuery)
    {
        m_IncomingPlayers.clear();
        m_hPlayersQuery = SteamMatchmakingServers()->PlayerDetails(m_iIP, m_iQueryPort, this);
    }
}

void CServerInfoQuery::ServerFailedToRespond()
{
    m_hPingQuery = 0;
    m_bNotResponding = true;
    m_iResponses++;
}

void CServerInfoQuery::AddPlayerToList(const char* name, int score, float seconds)
{
    m_IncomingPlayers.push_back({ name, score, seconds });
}

void CServerInfoQuery::PlayersFailedToRespond()
{
    m_hPlayersQuery = 0;
    m_Players.clear();
}

void CServerInfoQuery::PlayersRefreshComplete()
{
    m_hPlayersQuery = 0;
    m_Players = std::move(m_IncomingPlayers);
    m_IncomingPlayers.clear();
}
