#include "GameUi.h"
#include "GameUINext.h"
#include "ImGuiServerBrowser.h"
#include "ServerBrowserDialog.h"
#include "ServerBrowser/ServerGameModeNames.h"
#include <GameServerHelpers.h>
#include <ModInfo.h>

#include <tier1/strtools.h>
#include <FileSystem.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Controls.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cfloat>
#include <ctime>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui2;

namespace
{
    // the user id of each table column, which its sort spec reports
    enum Column
    {
        kColumnPassword,
        kColumnSecure,
        kColumnName,
        kColumnMode,
        kColumnPlayers,
        kColumnBots,
        kColumnMap,
        kColumnPing,
        kColumnCountry,
        kColumnAddress,
        kColumnLastPlayed,
    };

    // how often a list that is still being filled re-sorts, in seconds
    constexpr double kRebuildInterval = 0.3;

    // a server that never answered has no ping, and goes after every one that did
    int PingOf(const gameserveritem_t& server)
    {
        return server.m_bHadSuccessfulResponse ? server.m_nPing : INT_MAX;
    }

    struct SortEntry
    {
        int id;
        const serveritem_t* server;
        std::string name;
    };

    int Compare(const SortEntry& a, const SortEntry& b, int column)
    {
        const gameserveritem_t& x = a.server->gs;
        const gameserveritem_t& y = b.server->gs;

        switch (column)
        {
            case kColumnPassword: return int(x.m_bPassword) - int(y.m_bPassword);
            case kColumnSecure: return int(x.m_bSecure) - int(y.m_bSecure);
            case kColumnName: return V_stricmp(a.name.c_str(), b.name.c_str());
            case kColumnMode: return V_stricmp(a.server->next_details.game_mode, b.server->next_details.game_mode);
            case kColumnPlayers:
                if (GetHumanPlayerCount(x) != GetHumanPlayerCount(y))
                    return GetHumanPlayerCount(x) - GetHumanPlayerCount(y);
                return x.m_nMaxPlayers - y.m_nMaxPlayers;
            case kColumnBots: return x.m_nBotPlayers - y.m_nBotPlayers;
            case kColumnMap: return V_stricmp(x.m_szMap, y.m_szMap);
            case kColumnPing: return PingOf(x) < PingOf(y) ? -1 : PingOf(x) > PingOf(y);
            case kColumnCountry: return V_stricmp(a.server->next_details.country_code, b.server->next_details.country_code);
            case kColumnAddress:
                if (x.m_NetAdr.GetIP() != y.m_NetAdr.GetIP())
                    return x.m_NetAdr.GetIP() < y.m_NetAdr.GetIP() ? -1 : 1;
                return x.m_NetAdr.GetConnectionPort() - y.m_NetAdr.GetConnectionPort();
            case kColumnLastPlayed:
                return x.m_ulTimeLastPlayed < y.m_ulTimeLastPlayed ? -1 : x.m_ulTimeLastPlayed > y.m_ulTimeLastPlayed;
            default: return 0;
        }
    }

    void DrawIcon(int texture)
    {
        float size = ImGui::GetTextLineHeight();
        ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture)), ImVec2(size, size));
    }

    // for the tokens only NextClient's own files have, which not every language has yet
    std::string LocalizedOr(const char* token, const char* english)
    {
        std::string text = CImGuiPanel::Localized(token);
        return text.empty() ? english : text;
    }

    std::string FormatLastPlayed(uint32_t unixTime)
    {
        if (!unixTime)
            return {};

        time_t time = unixTime;
        char text[64];
        strftime(text, sizeof(text), "%a %e %b %H:%M", localtime(&time));
        return text;
    }
}

CImGuiServerBrowser::CImGuiServerBrowser() : BaseClass("serverbrowser_layout.ini")
{
    SetVisible(false);

    m_Tabs.push_back(std::make_unique<Tab>(ServerBrowserTab::Internet, "#ServerBrowser_InternetTab", GuiConnectionSource::ServersInternet));
    m_Tabs.push_back(std::make_unique<Tab>(ServerBrowserTab::Favorites, "#ServerBrowser_FavoritesTab", GuiConnectionSource::ServersFavorites));
    m_Tabs.push_back(std::make_unique<Tab>(ServerBrowserTab::History, "#ServerBrowser_HistoryTab", GuiConnectionSource::ServersHistory));
    m_Tabs.push_back(std::make_unique<Tab>(ServerBrowserTab::LAN, "#ServerBrowser_LanTab", GuiConnectionSource::Unknown));
    m_Tabs.push_back(std::make_unique<Tab>(ServerBrowserTab::Friends, "#ServerBrowser_FriendsTab", GuiConnectionSource::Unknown));
}

void CImGuiServerBrowser::Activate()
{
    SetVisible(true);
    MoveToFront();
    RequestFocus();
    ResetInput();
    m_bFocusWindow = true;
}

void CImGuiServerBrowser::Activate(ServerBrowserTab tab)
{
    for (auto& candidate : m_Tabs)
    {
        if (candidate->id == tab)
            m_pTabToSelect = candidate.get();
    }

    Activate();
}

void CImGuiServerBrowser::Close()
{
    for (auto& tab : m_Tabs)
        tab->servers.StopRefresh(IGameList::CancelQueryReason::PageClosed);

    SetVisible(false);
}

void CImGuiServerBrowser::Request(Tab& tab)
{
    // the steam interface wants an array even when it's empty
    MatchMakingKeyValuePair_t* noFilters[1] = {};

    tab.requested = true;
    tab.rows.clear();
    tab.selected = -1;

    switch (tab.id)
    {
        case ServerBrowserTab::Internet: tab.servers.RequestInternet(noFilters, 0); break;
        case ServerBrowserTab::Favorites: tab.servers.RequestFavorites(noFilters, 0); break;
        case ServerBrowserTab::History: tab.servers.RequestHistory(noFilters, 0); break;
        case ServerBrowserTab::LAN: tab.servers.RequestLan(); break;
        case ServerBrowserTab::Friends: tab.servers.RequestFriends(noFilters, 0); break;
        default: break;
    }
}

const std::string& CImGuiServerBrowser::GameModeText(const char* mode)
{
    auto found = m_GameModeTexts.find(mode);
    if (found != m_GameModeTexts.end())
        return found->second;

    const char* token = ServerGameMode_GetNameToken(mode);
    std::string text = token ? Localized(token) : std::string();
    return m_GameModeTexts.emplace(mode, text.empty() ? mode : text).first->second;
}

int CImGuiServerBrowser::Texture(const std::string& path)
{
    auto found = m_Textures.find(path);
    if (found != m_Textures.end())
        return found->second;

    // the engine's surface uploads the TGA into the GL texture of that id, which ImGui can draw as is
    int texture = 0;
    if (g_pFullFileSystem->FileExists((path + ".tga").c_str()))
    {
        texture = surface()->CreateNewTextureID();
        surface()->DrawSetTextureFile(texture, path.c_str(), true, false);
    }

    m_Textures.emplace(path, texture);
    return texture;
}

int CImGuiServerBrowser::FlagTexture(const char* countryCode)
{
    if (!countryCode[0])
        return 0;

    std::string code = countryCode;
    V_strlower(code.data());
    return Texture("servers/flags/" + code);
}

int CImGuiServerBrowser::HeaderIcon(int column)
{
    switch (column)
    {
        case kColumnPassword: return Texture("servers/icon_password_column");
        case kColumnSecure: return Texture("servers/icon_robotron_column");
        // icon_bots_column is a plain square that tells nothing, the old browser kept its column hidden
        case kColumnBots: return Texture("servers/icon_bots");
        case kColumnCountry: return Texture("servers/icon_country_column");
        default: return 0;
    }
}

void CImGuiServerBrowser::DrawHeaders()
{
    // TableHeadersRow only writes names; the icon columns get a nameless header with their picture over it
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    for (int column = 0; column < ImGui::TableGetColumnCount(); column++)
    {
        if (!ImGui::TableSetColumnIndex(column))
            continue;

        const char* name = ImGui::TableGetColumnName(column);
        int icon = HeaderIcon(column);

        ImGui::PushID(column);
        ImGui::TableHeader(icon ? "" : name);
        ImGui::PopID();

        if (!icon)
            continue;

        float size = ImGui::GetTextLineHeight();
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        ImVec2 corner((min.x + max.x - size) * 0.5f, (min.y + max.y - size) * 0.5f);
        ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(icon)), corner,
                                             ImVec2(corner.x + size, corner.y + size));

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", name);
    }
}

bool CImGuiServerBrowser::PassesFilters(const Tab& tab, const serveritem_t& server) const
{
    const gameserveritem_t& gs = server.gs;

    // the internet list only shows the servers that answered; the saved lists keep theirs
    if (tab.id == ServerBrowserTab::Internet && !server.hadSuccessfulResponse)
        return false;

    // a server of another game; one that never answered has no game dir yet
    const char* modDir = ServerBrowserDialog().GetActiveModName();
    if (modDir && gs.m_szGameDir[0] && V_stricmp(modDir, gs.m_szGameDir))
        return false;

    if (m_bHideEmpty && GetHumanPlayerCount(gs) < 1)
        return false;
    if (m_bHideFull && IsServerFull(gs))
        return false;
    if (m_bHidePassworded && gs.m_bPassword)
        return false;

    if (m_szSearch[0])
    {
        bool found = V_stristr(gs.GetName().c_str(), m_szSearch) || V_stristr(gs.m_szMap, m_szSearch) ||
                     V_stristr(gs.m_NetAdr.GetConnectionAddressString().c_str(), m_szSearch);
        if (!found)
            return false;
    }

    return true;
}

void CImGuiServerBrowser::RebuildRows(Tab& tab, ImGuiTableSortSpecs* sortSpecs)
{
    std::vector<SortEntry> entries;
    for (auto& [id, server] : tab.servers)
    {
        if (PassesFilters(tab, server))
            entries.push_back({ id, &server, server.gs.GetName() });
    }

    auto less = [sortSpecs](const SortEntry& a, const SortEntry& b)
    {
        for (int i = 0; sortSpecs && i < sortSpecs->SpecsCount; i++)
        {
            const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[i];
            int order = Compare(a, b, static_cast<int>(spec.ColumnUserID));
            if (order != 0)
                return spec.SortDirection == ImGuiSortDirection_Ascending ? order < 0 : order > 0;
        }

        // the server ids keep equal rows from swapping places on every rebuild
        return a.id < b.id;
    };
    std::sort(entries.begin(), entries.end(), less);

    tab.rows.clear();
    for (const SortEntry& entry : entries)
        tab.rows.push_back(entry.id);

    tab.rowsRevision = tab.servers.get_revision();
}

void CImGuiServerBrowser::MoveSelection(Tab& tab, int step)
{
    if (tab.rows.empty())
        return;

    auto current = std::find(tab.rows.begin(), tab.rows.end(), tab.selected);
    int index = current == tab.rows.end() ? 0 : static_cast<int>(current - tab.rows.begin()) + step;
    tab.selected = tab.rows[std::clamp(index, 0, static_cast<int>(tab.rows.size()) - 1)];
}

void CImGuiServerBrowser::Connect(Tab& tab, int serverID)
{
    if (!tab.servers.IsServerExists(serverID))
        return;

    const gameserveritem_t& server = tab.servers.GetServer(serverID).gs;
    if (server.m_bPassword)
    {
        m_PasswordServer = server;
        m_PasswordSource = tab.source;
        m_szPassword[0] = '\0';
        m_bOpenPasswordPopup = true;
        return;
    }

    ConnectWithPassword(server, tab.source, "");
}

void CImGuiServerBrowser::ConnectWithPassword(const gameserveritem_t& server, GuiConnectionSource source, const char* password)
{
    for (auto& tab : m_Tabs)
        tab->servers.StopRefresh(IGameList::CancelQueryReason::ConnectToServer);

    GameUINext().SetLastConnectionInfo(server.m_NetAdr, source, server.m_szMap);

    char command[256];
    if (password[0])
    {
        // the password needs room in the userinfo, and Counter-Strike doesn't use the model key
        if (!V_stricmp(ModInfo().GetGameDescription(), "Counter-Strike"))
            engine->pfnClientCmd("setinfo model \"\"\n");

        V_snprintf(command, sizeof(command), "password \"%s\"\n", password);
        engine->pfnClientCmd(command);
    }

    V_snprintf(command, sizeof(command), "connect %s\n", server.m_NetAdr.GetConnectionAddressString().c_str());
    engine->pfnClientCmd(command);
}

void CImGuiServerBrowser::DrawImGui()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(960, 640), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(640, 384), ImVec2(FLT_MAX, FLT_MAX));

    if (m_bFocusWindow)
    {
        ImGui::SetNextWindowFocus();
        m_bFocusWindow = false;
    }

    bool open = true;
    std::string title = Localized("#ServerBrowser_Servers") + "###ServerBrowser";
    if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse))
    {
        Tab* active = nullptr;
        if (ImGui::BeginTabBar("Tabs"))
        {
            for (auto& tab : m_Tabs)
            {
                ImGuiTabItemFlags flags = m_pTabToSelect == tab.get() ? ImGuiTabItemFlags_SetSelected : 0;
                std::string label = Localized(tab->token) + "###" + tab->token;
                if (ImGui::BeginTabItem(label.c_str(), nullptr, flags))
                {
                    active = tab.get();
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
        m_pTabToSelect = nullptr;

        // a tab that was left stops refreshing, like the old browser's pages did
        if (m_pActiveTab && m_pActiveTab != active)
            m_pActiveTab->servers.StopRefresh(IGameList::CancelQueryReason::PageClosed);
        m_pActiveTab = active;

        if (active)
        {
            if (!active->requested)
                Request(*active);

            DrawToolbar(*active);

            float footer = ImGui::GetFrameHeightWithSpacing();
            ImGui::BeginChild("List", ImVec2(0, -footer));
            DrawTable(*active);
            ImGui::EndChild();

            DrawStatus(*active);
        }

        DrawPasswordPopup();

        // a popup takes its own Escape; in a level, Escape belongs to the game menu
        bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
        if (focused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape) && !GameUI().IsInLevel())
            open = false;
    }
    ImGui::End();

    if (!open)
        Close();
}

void CImGuiServerBrowser::DrawToolbar(Tab& tab)
{
    if (tab.servers.IsRefreshing())
    {
        if (ImGui::Button(Localized("#ServerBrowser_StopRefreshingList").c_str()))
            tab.servers.StopRefresh(IGameList::CancelQueryReason::UserCancellation);
    }
    else if (ImGui::Button(Localized("#ServerBrowser_Refresh").c_str()))
    {
        Request(tab);
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(tab.selected < 0);
    if (ImGui::Button(Localized("#ServerBrowser_Connect").c_str()))
        Connect(tab, tab.selected);
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    std::string hint = LocalizedOr("#ServerBrowser_Search", "Name, map or address");
    if (ImGui::InputTextWithHint("##Search", hint.c_str(), m_szSearch, sizeof(m_szSearch)))
        m_bFiltersChanged = true;

    m_bFiltersChanged |= ImGui::Checkbox(Localized("#ServerBrowser_HasUsersPlaying").c_str(), &m_bHideEmpty);
    ImGui::SameLine();
    m_bFiltersChanged |= ImGui::Checkbox(Localized("#ServerBrowser_ServerNotFull").c_str(), &m_bHideFull);
    ImGui::SameLine();
    m_bFiltersChanged |= ImGui::Checkbox(Localized("#ServerBrowser_IsNotPasswordProtected").c_str(), &m_bHidePassworded);
}

void CImGuiServerBrowser::DrawTable(Tab& tab)
{
    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    bool history = tab.id == ServerBrowserTab::History;
    int columns = history ? 11 : 10;

    // every tab keeps its own column widths, order and sorting
    ImGui::PushID(tab.token);
    if (!ImGui::BeginTable("ServerTable", columns, flags))
    {
        ImGui::PopID();
        return;
    }

    // the list starts empty, so the widths can't be fitted to the servers yet; a text column is
    // also as wide as its name and the sort arrow next to it
    float charWidth = ImGui::CalcTextSize("0").x;
    auto setupColumn = [charWidth](const char* token, ImGuiTableColumnFlags flags, int chars, Column column)
    {
        std::string name = Localized(token);
        float width = std::max(charWidth * chars, ImGui::CalcTextSize(name.c_str()).x + ImGui::GetFontSize());
        ImGui::TableSetupColumn(name.c_str(), flags, width, column);
    };
    auto setupIconColumn = [](const char* token, int chars, Column column)
    {
        float width = std::max(ImGui::GetTextLineHeight(), ImGui::CalcTextSize("0").x * chars);
        ImGui::TableSetupColumn(Localized(token).c_str(), ImGuiTableColumnFlags_NoResize, width, column);
    };

    ImGui::TableSetupScrollFreeze(0, 1);
    setupIconColumn("#ServerBrowser_Password", 0, kColumnPassword);
    setupIconColumn("#ServerBrowser_Secure", 0, kColumnSecure);
    ImGui::TableSetupColumn(Localized("#ServerBrowser_Servers").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.0f, kColumnName);
    setupColumn("#ServerBrowser_GameMode", 0, 10, kColumnMode);
    setupColumn("#ServerBrowser_Players", 0, 7, kColumnPlayers);
    // two digits of bots under the icon
    setupIconColumn("#ServerBrowser_Bots", 2, kColumnBots);
    setupColumn("#ServerBrowser_Map", 0, 14, kColumnMap);
    setupColumn("#ServerBrowser_Latency", ImGuiTableColumnFlags_DefaultSort, 4, kColumnPing);
    setupIconColumn("#ServerBrowser_Country", 0, kColumnCountry);
    setupColumn("#ServerBrowser_IPAddress", ImGuiTableColumnFlags_DefaultHide, 21, kColumnAddress);
    if (history)
        setupColumn("#ServerBrowser_LastPlayed", 0, 16, kColumnLastPlayed);
    DrawHeaders();

    // a list that is still filling re-sorts a few times a second, not on every answer
    ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();
    double now = system()->GetCurrentTime();
    bool sortChanged = sortSpecs && sortSpecs->SpecsDirty;
    bool listChanged = tab.rowsRevision != tab.servers.get_revision() && now >= m_flNextRebuildTime;
    if (sortChanged || listChanged || m_bFiltersChanged)
    {
        RebuildRows(tab, sortSpecs);
        if (sortSpecs)
            sortSpecs->SpecsDirty = false;
        m_flNextRebuildTime = now + kRebuildInterval;
    }

    // the filters are shared, so the other tabs rebuild once they're shown
    if (m_bFiltersChanged)
    {
        for (auto& other : m_Tabs)
            other->rowsRevision = UINT32_MAX;
        tab.rowsRevision = tab.servers.get_revision();
        m_bFiltersChanged = false;
    }

    // the keyboard moves the selection while nothing is being typed
    bool scrollToSelected = false;
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput &&
        !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId))
    {
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            MoveSelection(tab, -1), scrollToSelected = true;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            MoveSelection(tab, 1), scrollToSelected = true;
        if ((ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) && tab.selected >= 0)
            Connect(tab, tab.selected);
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(tab.rows.size()));
    if (scrollToSelected)
    {
        auto selected = std::find(tab.rows.begin(), tab.rows.end(), tab.selected);
        if (selected != tab.rows.end())
            clipper.IncludeItemByIndex(static_cast<int>(selected - tab.rows.begin()));
    }

    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
        {
            int id = tab.rows[row];
            if (!tab.servers.IsServerExists(id))
                continue;

            ImGui::PushID(id);
            DrawRow(tab, tab.servers.GetServer(id));
            if (scrollToSelected && id == tab.selected)
                ImGui::SetScrollHereY();
            ImGui::PopID();
        }
    }

    ImGui::EndTable();
    ImGui::PopID();
}

void CImGuiServerBrowser::DrawRow(Tab& tab, const serveritem_t& server)
{
    const gameserveritem_t& gs = server.gs;
    ImGui::TableNextRow();

    // the row's selectable goes into its first visible column, since a hidden one isn't drawn
    bool selectableDrawn = false;
    for (int column = 0; column < ImGui::TableGetColumnCount(); column++)
    {
        if (!ImGui::TableSetColumnIndex(column))
            continue;

        if (!selectableDrawn)
        {
            selectableDrawn = true;
            ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick |
                                         ImGuiSelectableFlags_AllowOverlap;
            if (ImGui::Selectable("##Row", tab.selected == server.serverID, flags))
            {
                if (tab.selected != server.serverID && tab.id == ServerBrowserTab::Internet)
                {
                    int index = static_cast<int>(std::find(tab.rows.begin(), tab.rows.end(), server.serverID) - tab.rows.begin());
                    GameUINext().InvokeInternetServerSelected(gs.m_NetAdr.GetIP(), gs.m_NetAdr.GetConnectionPort(), index + 1, static_cast<int>(tab.rows.size()));
                }

                tab.selected = server.serverID;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    Connect(tab, server.serverID);
            }

            if (ImGui::BeginPopupContextItem("Context"))
            {
                tab.selected = server.serverID;
                DrawContextMenu(tab);
                ImGui::EndPopup();
            }
            ImGui::SameLine(0.0f, 0.0f);
        }

        // the columns were set up in the order of Column, so the index is the Column
        switch (column)
        {
            case kColumnPassword:
                if (gs.m_bPassword)
                    DrawIcon(Texture("servers/icon_password"));
                break;
            case kColumnSecure:
                if (gs.m_bSecure)
                    ImGui::TextUnformatted("✔");
                break;
            case kColumnName:
                ImGui::TextUnformatted(gs.GetName().c_str());
                break;
            case kColumnMode:
                if (server.next_details.game_mode[0])
                    ImGui::TextUnformatted(GameModeText(server.next_details.game_mode).c_str());
                break;
            case kColumnPlayers:
                if (gs.m_bHadSuccessfulResponse)
                    ImGui::Text("%d / %d", GetHumanPlayerCount(gs), gs.m_nMaxPlayers);
                else
                    ImGui::TextDisabled("-");
                break;
            case kColumnBots:
                if (gs.m_nBotPlayers > 0)
                    ImGui::Text("%d", gs.m_nBotPlayers);
                break;
            case kColumnMap:
                ImGui::TextUnformatted(gs.m_szMap);
                break;
            case kColumnPing:
                if (gs.m_bHadSuccessfulResponse)
                    ImGui::Text("%d", gs.m_nPing);
                else
                    ImGui::TextDisabled("-");
                break;
            case kColumnCountry:
                // the flag sits in the middle of a 16x16 picture with transparent margins
                if (int texture = FlagTexture(server.next_details.country_code))
                    DrawIcon(texture);
                else
                {
                    ImGui::TextUnformatted(server.next_details.country_code);
                }

                if (server.next_details.country_code[0] && ImGui::IsItemHovered())
                {
                    const char* name = server.next_details.country_name;
                    ImGui::SetTooltip("%s", name[0] ? name : server.next_details.country_code);
                }
                break;
            case kColumnAddress:
                ImGui::TextUnformatted(gs.m_NetAdr.GetConnectionAddressString().c_str());
                break;
            case kColumnLastPlayed:
                ImGui::TextUnformatted(FormatLastPlayed(gs.m_ulTimeLastPlayed).c_str());
                break;
        }
    }
}

void CImGuiServerBrowser::DrawContextMenu(Tab& tab)
{
    if (!tab.servers.IsServerExists(tab.selected))
        return;

    const gameserveritem_t& server = tab.servers.GetServer(tab.selected).gs;

    if (ImGui::MenuItem(Localized("#ServerBrowser_ConnectToServer").c_str()))
        Connect(tab, tab.selected);

    if (ImGui::MenuItem(Localized("#ServerBrowser_RefreshServer").c_str(), nullptr, false, !tab.servers.IsRefreshing()))
        tab.servers.StartRefreshServer(tab.selected);

    if (ImGui::MenuItem(LocalizedOr("#ServerBrowser_CopyAddress", "Copy address").c_str()))
        ImGui::SetClipboardText(server.m_NetAdr.GetConnectionAddressString().c_str());

    if (tab.id != ServerBrowserTab::Favorites && ImGui::MenuItem(Localized("#ServerBrowser_AddServerToFavorites").c_str()))
    {
        ServerBrowserDialog().AddServerToFavorites(server);

        // the favorites tab asks Steam again once it's opened
        for (auto& other : m_Tabs)
        {
            if (other->id == ServerBrowserTab::Favorites)
                other->requested = false;
        }
    }
}

void CImGuiServerBrowser::DrawStatus(Tab& tab)
{
    int total = static_cast<int>(tab.servers.ServerCount());
    int answered = static_cast<int>(tab.servers.AnsweredCount());

    if (tab.servers.IsRefreshing() && total > 0 && answered < total)
    {
        char overlay[64];
        V_snprintf(overlay, sizeof(overlay), "%d / %d", answered, total);
        ImGui::ProgressBar(static_cast<float>(answered) / static_cast<float>(total), ImVec2(-FLT_MIN, 0), overlay);
        return;
    }

    if (tab.servers.IsRefreshing())
        ImGui::TextUnformatted(Localized("#ServerBrowser_GettingNewServerList").c_str());
    else
        ImGui::Text("%s: %d / %d", Localized("#ServerBrowser_Servers").c_str(), static_cast<int>(tab.rows.size()), answered);
}

void CImGuiServerBrowser::DrawPasswordPopup()
{
    const char* popupId = "###Password";
    if (m_bOpenPasswordPopup)
    {
        ImGui::OpenPopup(popupId);
        m_bOpenPasswordPopup = false;
    }

    // the panel only covers the windows, so dimming the rest of the screen would be all there is to see of it
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, IM_COL32(0, 0, 0, 0));
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    std::string title = Localized("#ServerBrowser_Password") + popupId;
    if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(m_PasswordServer.GetName().c_str());
        ImGui::TextUnformatted(Localized("#ServerBrowser_PasswordRequired").c_str());

        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool submitted = ImGui::InputText("##Password", m_szPassword, sizeof(m_szPassword),
                                          ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);

        submitted |= ImGui::Button(Localized("#ServerBrowser_Connect").c_str());
        ImGui::SameLine();
        bool cancelled = ImGui::Button(Localized("#ServerBrowser_Cancel").c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape);

        if (submitted && m_szPassword[0])
        {
            ConnectWithPassword(m_PasswordServer, m_PasswordSource, m_szPassword);
            ImGui::CloseCurrentPopup();
        }
        else if (cancelled)
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}
