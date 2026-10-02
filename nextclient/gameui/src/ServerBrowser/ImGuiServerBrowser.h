#pragma once

#include "ImGuiPanel.h"
#include "IServerRefreshResponse.h"
#include "ServerFilterCounts.h"
#include "ServerInfoQuery.h"
#include "ServerList.h"
#include "../IServerBrowserEx.h"

#include <next_gameui/IGameUiNext.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct ImGuiTableSortSpecs;

// The server browser drawn with Dear ImGui; sb_legacy 1 brings the VGUI one back
class CImGuiServerBrowser : public CImGuiPanel
{
    DECLARE_CLASS_SIMPLE(CImGuiServerBrowser, CImGuiPanel);

public:
    CImGuiServerBrowser();

    // the cvars keep the filters in config.cfg, so they have to exist before it runs
    static void RegisterCvars();

    void Activate();
    void Activate(ServerBrowserTab tab);

protected:
    void DrawImGui() override;

private:
    // CServerList reports to a response target; the browser reads the list's revision instead
    struct Tab : IServerRefreshResponse
    {
        Tab(ServerBrowserTab id, const char* token, GuiConnectionSource source) : id(id), token(token), source(source) {}

        void ServerResponded(serveritem_t&) override {}
        void ServerFailedToRespond(serveritem_t&) override {}
        void RefreshComplete() override {}

        ServerBrowserTab id;
        const char* token;
        GuiConnectionSource source;
        CServerList servers{ this };
        bool requested = false;

        // ids of the servers that pass the filters, in the table's order
        std::vector<int> rows;
        uint32_t rowsRevision = UINT32_MAX;
        // what each mode and country would list, counted with the rows
        ServerFilterCounts counts;
        int selected = -1;
    };

    void DrawToolbar(Tab& tab);
    void DrawModeFilter(const Tab& tab, float width);
    void DrawCountryFilter(const Tab& tab, float width);
    void DrawPingFilter(float width);
    void DrawTable(Tab& tab);
    void DrawRow(Tab& tab, const serveritem_t& server);
    void DrawContextMenu(Tab& tab);
    void DrawStatus(Tab& tab);
    void DrawPasswordPopup();
    void DrawServerInfo();
    void DrawServerDetails(const gameserveritem_t& server);
    // height as ImGui takes it, negative for all but that much of the window
    void DrawPlayers(const CServerInfoQuery& query, float height);

    void Request(Tab& tab);
    void RebuildRows(Tab& tab, ImGuiTableSortSpecs* sortSpecs);
    // every filter but the mode and country ones, which are counted apart
    bool PassesBaseFilters(const Tab& tab, const serveritem_t& server) const;
    bool PassesModeFilter(const ServerDetailsNext& details) const;
    bool PassesCountryFilter(const ServerDetailsNext& details) const;
    void LoadFilters();
    void SaveFilters();
    void OnFiltersChanged();
    void MoveSelection(Tab& tab, int step);
    void Connect(Tab& tab, int serverID);
    // a full server opens its info instead, where auto-retry can wait for a free slot
    void JoinServer(const serveritem_t& server, GuiConnectionSource source);
    void OpenServerInfo(const serveritem_t& server, GuiConnectionSource source);
    void ConnectWithPassword(const gameserveritem_t& server, GuiConnectionSource source, const char* password);
    void Close();

    const std::string& GameModeText(const char* mode);
    // a TGA of the game's files without its extension, 0 when it's missing
    int Texture(const std::string& path);
    int FlagTexture(const char* countryCode);
    // the picture a column header shows instead of its name, 0 for a column with a text header
    int HeaderIcon(int column);
    void DrawHeaders();

    std::vector<std::unique_ptr<Tab>> m_Tabs;
    Tab* m_pActiveTab = nullptr;
    // set by Activate(tab), for the tab bar to pick on the next frame
    Tab* m_pTabToSelect = nullptr;
    bool m_bFocusWindow = false;

    char m_szSearch[128] = {};
    bool m_bHideEmpty = false;
    bool m_bHideFull = false;
    bool m_bHidePassworded = false;
    // a mode identifier and an upper-case country code, empty for all
    std::string m_ModeFilter;
    std::string m_CountryFilter;
    // the highest ping to show, 0 for any
    int m_iPingFilter = 0;
    char m_szCountrySearch[64] = {};
    bool m_bFiltersLoaded = false;
    // UTF-8 names of the countries the lists had, by code
    std::map<std::string, std::string> m_CountryNames;
    // the filters changed, so every tab's rows have to be rebuilt
    bool m_bFiltersChanged = false;
    double m_flNextRebuildTime = 0.0;

    // the server waiting for its password, copied since a refresh may replace the list's entry
    bool m_bOpenPasswordPopup = false;
    gameserveritem_t m_PasswordServer{};
    GuiConnectionSource m_PasswordSource = GuiConnectionSource::Unknown;
    char m_szPassword[64] = {};

    // the server info window, open while there is a query
    std::unique_ptr<CServerInfoQuery> m_pServerInfo;
    // the mode and country the list had, which a ping doesn't tell
    ServerDetailsNext m_InfoDetails{};
    GuiConnectionSource m_InfoSource = GuiConnectionSource::Unknown;
    bool m_bInfoAppearing = false;
    bool m_bInfoFull = false;
    bool m_bAutoJoin = false;
    double m_flNextInfoRetry = 0.0;
    uint32_t m_iInfoResponsesSeen = 0;

    std::unordered_map<std::string, std::string> m_GameModeTexts;
    // by path
    std::unordered_map<std::string, int> m_Textures;
};
