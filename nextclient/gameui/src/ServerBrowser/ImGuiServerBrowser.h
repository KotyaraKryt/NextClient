#pragma once

#include "ImGuiPanel.h"
#include "IServerRefreshResponse.h"
#include "ServerList.h"
#include "../IServerBrowserEx.h"

#include <next_gameui/IGameUiNext.h>

#include <cstdint>
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
        int selected = -1;
    };

    void DrawToolbar(Tab& tab);
    void DrawTable(Tab& tab);
    void DrawRow(Tab& tab, const serveritem_t& server);
    void DrawContextMenu(Tab& tab);
    void DrawStatus(Tab& tab);
    void DrawPasswordPopup();

    void Request(Tab& tab);
    void RebuildRows(Tab& tab, ImGuiTableSortSpecs* sortSpecs);
    bool PassesFilters(const Tab& tab, const serveritem_t& server) const;
    void MoveSelection(Tab& tab, int step);
    void Connect(Tab& tab, int serverID);
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
    // the filters changed, so every tab's rows have to be rebuilt
    bool m_bFiltersChanged = false;
    double m_flNextRebuildTime = 0.0;

    // the server waiting for its password, copied since a refresh may replace the list's entry
    bool m_bOpenPasswordPopup = false;
    gameserveritem_t m_PasswordServer{};
    GuiConnectionSource m_PasswordSource = GuiConnectionSource::Unknown;
    char m_szPassword[64] = {};

    std::unordered_map<std::string, std::string> m_GameModeTexts;
    // by path
    std::unordered_map<std::string, int> m_Textures;
};
