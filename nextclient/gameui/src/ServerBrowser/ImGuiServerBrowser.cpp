#include "GameUi.h"
#include "GameUINext.h"
#include "ImGuiServerBrowser.h"
#include "ServerBrowserDialog.h"
#include "ServerBrowser/ServerBrowserText.h"
#include "ServerBrowser/ServerGameModeNames.h"
#include <GameServerHelpers.h>
#include <ModInfo.h>

#include <cvardef.h>
#include <nitro_utils/string_utils.h>
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

    // the limits the ping filter offers, with their names in the stock localization
    struct PingLimit
    {
        int ping;
        const char* token;
    };

    constexpr PingLimit kPingLimits[] = {
        { 50, "#ServerBrowser_LessThan50" },
        { 100, "#ServerBrowser_LessThan100" },
        { 150, "#ServerBrowser_LessThan150" },
        { 250, "#ServerBrowser_LessThan250" },
        { 350, "#ServerBrowser_LessThan350" },
        { 600, "#ServerBrowser_LessThan600" },
    };

    const char* CvarString(const char* name)
    {
        cvar_t* cvar = engine->pfnGetCvarPointer(name);
        return cvar ? cvar->string : "";
    }

    float CvarValue(const char* name)
    {
        cvar_t* cvar = engine->pfnGetCvarPointer(name);
        return cvar ? cvar->value : 0.0f;
    }

    // how often the info window asks a full server again while auto-retry waits, in seconds
    constexpr double kInfoRetryInterval = 2.5;

    std::string FormatPlayedTime(float seconds)
    {
        int total = static_cast<int>(seconds);
        int hours = total / 3600;
        int minutes = total / 60 % 60;
        char text[32];
        if (hours)
            V_snprintf(text, sizeof(text), "%dh %dm %ds", hours, minutes, total % 60);
        else if (minutes)
            V_snprintf(text, sizeof(text), "%dm %ds", minutes, total % 60);
        else
            V_snprintf(text, sizeof(text), "%ds", total);
        return text;
    }

    // muted to sit on the olive theme: a good ping, a playable one, a bad one, and a warning
    const ImVec4 kGoodColor(0.60f, 0.78f, 0.45f, 1.0f);
    const ImVec4 kFairColor(0.86f, 0.78f, 0.40f, 1.0f);
    const ImVec4 kBadColor(0.88f, 0.52f, 0.40f, 1.0f);
    const ImVec4 kTitleColor(0.92f, 0.88f, 0.62f, 1.0f);

    void DrawPing(const gameserveritem_t& server)
    {
        if (!server.m_bHadSuccessfulResponse)
        {
            ImGui::TextDisabled("-");
            return;
        }

        int ping = server.m_nPing;
        ImGui::TextColored(ping < 80 ? kGoodColor : ping < 150 ? kFairColor : kBadColor, "%d", ping);
    }

    // a full server's count stands out, since joining it needs a free slot
    void DrawPlayerCount(const gameserveritem_t& server)
    {
        if (!server.m_bHadSuccessfulResponse)
        {
            ImGui::TextDisabled("-");
            return;
        }

        if (IsServerFull(server))
            ImGui::TextColored(kBadColor, "%d / %d", GetHumanPlayerCount(server), server.m_nMaxPlayers);
        else
            ImGui::Text("%d / %d", GetHumanPlayerCount(server), server.m_nMaxPlayers);
    }

    // at least minWidth, and wide enough for what the closed combo shows next to its arrow
    float ComboWidth(const std::string& preview, float minWidth)
    {
        float fits = ImGui::CalcTextSize(preview.c_str()).x + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetFrameHeight();
        return std::max(minWidth, fits);
    }

    std::string WithCount(const std::string& text, int count)
    {
        return text + " (" + std::to_string(count) + ")";
    }

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

void CImGuiServerBrowser::RegisterCvars()
{
    static const char* const kCvars[][2] = {
        { "sb_hideempty", "0" },
        { "sb_hidefull", "0" },
        { "sb_hidepassword", "0" },
        { "sb_mode", "" },
        { "sb_country", "" },
        { "sb_maxping", "0" },
    };
    for (const auto& cvar : kCvars)
        engine->pfnRegisterVariable(cvar[0], cvar[1], FCVAR_ARCHIVE);
}

void CImGuiServerBrowser::LoadFilters()
{
    m_bHideEmpty = CvarValue("sb_hideempty") != 0.0f;
    m_bHideFull = CvarValue("sb_hidefull") != 0.0f;
    m_bHidePassworded = CvarValue("sb_hidepassword") != 0.0f;
    m_ModeFilter = CvarString("sb_mode");
    m_CountryFilter = CvarString("sb_country");
    m_iPingFilter = static_cast<int>(CvarValue("sb_maxping"));
    m_bFiltersChanged = true;
}

void CImGuiServerBrowser::SaveFilters()
{
    engine->Cvar_SetValue("sb_hideempty", m_bHideEmpty ? 1.0f : 0.0f);
    engine->Cvar_SetValue("sb_hidefull", m_bHideFull ? 1.0f : 0.0f);
    engine->Cvar_SetValue("sb_hidepassword", m_bHidePassworded ? 1.0f : 0.0f);
    engine->Cvar_Set("sb_mode", m_ModeFilter.c_str());
    engine->Cvar_Set("sb_country", m_CountryFilter.c_str());
    engine->Cvar_SetValue("sb_maxping", static_cast<float>(m_iPingFilter));
}

void CImGuiServerBrowser::OnFiltersChanged()
{
    m_bFiltersChanged = true;
    SaveFilters();
}

void CImGuiServerBrowser::Activate()
{
    // config.cfg has run by the time the browser first opens, so the cvars hold the player's filters
    if (!m_bFiltersLoaded)
    {
        m_bFiltersLoaded = true;
        LoadFilters();
    }

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

    m_pServerInfo.reset();
    m_bAutoJoin = false;
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

bool CImGuiServerBrowser::PassesBaseFilters(const Tab& tab, const serveritem_t& server) const
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
    if (m_iPingFilter && (!gs.m_bHadSuccessfulResponse || gs.m_nPing > m_iPingFilter))
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

bool CImGuiServerBrowser::PassesModeFilter(const ServerDetailsNext& details) const
{
    return m_ModeFilter.empty() || ServerGameMode_MatchesFilter(details.game_mode, m_ModeFilter.c_str());
}

bool CImGuiServerBrowser::PassesCountryFilter(const ServerDetailsNext& details) const
{
    return m_CountryFilter.empty() || !V_stricmp(details.country_code, m_CountryFilter.c_str());
}

void CImGuiServerBrowser::RebuildRows(Tab& tab, ImGuiTableSortSpecs* sortSpecs)
{
    std::vector<SortEntry> entries;
    tab.counts = {};
    for (auto& [id, server] : tab.servers)
    {
        if (!PassesBaseFilters(tab, server))
            continue;

        const ServerDetailsNext& details = server.next_details;
        if (details.country_code[0] && details.country_name[0])
            m_CountryNames[details.country_code] = details.country_name;
        else if (details.country_code[0])
            m_CountryNames.emplace(details.country_code, details.country_code);

        // a mode counts the servers the country filter lets through, and the other way round
        bool passesMode = PassesModeFilter(details);
        bool passesCountry = PassesCountryFilter(details);
        ServerFilterCounts_Add(details, passesMode, passesCountry, &tab.counts);

        if (passesMode && passesCountry)
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

    JoinServer(tab.servers.GetServer(serverID), tab.source);
}

void CImGuiServerBrowser::JoinServer(const serveritem_t& server, GuiConnectionSource source)
{
    const gameserveritem_t& gs = server.gs;
    if (gs.m_bHadSuccessfulResponse && IsServerFull(gs))
    {
        bool sameServer = m_pServerInfo && m_pServerInfo->GetServer().m_NetAdr.GetIP() == gs.m_NetAdr.GetIP() &&
                          m_pServerInfo->GetServer().m_NetAdr.GetQueryPort() == gs.m_NetAdr.GetQueryPort();
        if (!sameServer)
            OpenServerInfo(server, source);
        m_bInfoFull = true;
        return;
    }

    if (gs.m_bPassword)
    {
        m_PasswordServer = gs;
        m_PasswordSource = source;
        m_szPassword[0] = '\0';
        m_bOpenPasswordPopup = true;
        return;
    }

    ConnectWithPassword(gs, source, "");
}

void CImGuiServerBrowser::OpenServerInfo(const serveritem_t& server, GuiConnectionSource source)
{
    m_pServerInfo = std::make_unique<CServerInfoQuery>(server.gs);
    m_InfoDetails = server.next_details;
    m_InfoSource = source;
    m_bInfoAppearing = true;
    m_bInfoFull = false;
    m_bAutoJoin = false;
    m_iInfoResponsesSeen = 0;
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

    // the info window has done its job once the connect is sent
    m_pServerInfo.reset();
    m_bAutoJoin = false;
}

void CImGuiServerBrowser::DrawImGui()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(960, 640), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(640, 384), ImVec2(FLT_MAX, FLT_MAX));

    // the stock browser's fields and checkboxes are outlined; without it a checkbox is a blank square
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

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

    DrawServerInfo();
    ImGui::PopStyleVar();

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
    // the search is for the moment, so it isn't kept with the other filters
    if (ImGui::InputTextWithHint("##Search", hint.c_str(), m_szSearch, sizeof(m_szSearch)))
        m_bFiltersChanged = true;

    float comboWidth = ImGui::CalcTextSize("0").x * 22;
    DrawModeFilter(tab, comboWidth);
    ImGui::SameLine();
    DrawCountryFilter(tab, comboWidth);
    ImGui::SameLine();
    DrawPingFilter(0.0f);

    // a narrow window moves the checkboxes that don't fit to the next row
    auto checkbox = [](const std::string& label, bool* value)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        float width = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize(label.c_str()).x;
        float right = ImGui::GetItemRectMax().x + style.ItemSpacing.x + width;
        if (right <= ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x)
            ImGui::SameLine();
        return ImGui::Checkbox(label.c_str(), value);
    };

    bool changed = false;
    changed |= checkbox(Localized("#ServerBrowser_HasUsersPlaying"), &m_bHideEmpty);
    changed |= checkbox(Localized("#ServerBrowser_ServerNotFull"), &m_bHideFull);
    changed |= checkbox(Localized("#ServerBrowser_IsNotPasswordProtected"), &m_bHidePassworded);
    if (changed)
        OnFiltersChanged();
}

void CImGuiServerBrowser::DrawModeFilter(const Tab& tab, float width)
{
    std::string all = Localized("#ServerBrowser_All");
    std::string preview = Localized("#ServerBrowser_GameMode") + ": " + (m_ModeFilter.empty() ? all : GameModeText(m_ModeFilter.c_str()));

    ImGui::SetNextItemWidth(ComboWidth(preview, width));
    if (!ImGui::BeginCombo("##Mode", preview.c_str()))
        return;

    if (ImGui::Selectable(WithCount(all, tab.counts.all_game_modes).c_str(), m_ModeFilter.empty()))
    {
        m_ModeFilter.clear();
        OnFiltersChanged();
    }

    std::vector<int> modes(std::size(kServerGameModeNames));
    for (size_t i = 0; i < modes.size(); i++)
        modes[i] = static_cast<int>(i);
    std::sort(modes.begin(), modes.end(), [this](int a, int b)
    {
        return V_stricmp(GameModeText(kServerGameModeNames[a].name).c_str(), GameModeText(kServerGameModeNames[b].name).c_str()) < 0;
    });

    for (int mode : modes)
    {
        const char* name = kServerGameModeNames[mode].name;
        int count = tab.counts.game_modes[mode];
        bool selected = !V_stricmp(name, m_ModeFilter.c_str());

        // a mode without servers stays pickable, its servers may still be on their way
        if (!count && !selected)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (ImGui::Selectable(WithCount(GameModeText(name), count).c_str(), selected))
        {
            m_ModeFilter = name;
            OnFiltersChanged();
        }
        if (!count && !selected)
            ImGui::PopStyleColor();
    }

    ImGui::EndCombo();
}

void CImGuiServerBrowser::DrawCountryFilter(const Tab& tab, float width)
{
    std::string all = Localized("#ServerBrowser_All");
    std::string current = all;
    if (!m_CountryFilter.empty())
    {
        auto name = m_CountryNames.find(m_CountryFilter);
        current = name != m_CountryNames.end() ? name->second : m_CountryFilter;
    }
    std::string preview = Localized("#ServerBrowser_Country") + ": " + current;

    ImGui::SetNextItemWidth(ComboWidth(preview, width));
    if (!ImGui::BeginCombo("##Country", preview.c_str(), ImGuiComboFlags_HeightLarge))
    {
        m_szCountrySearch[0] = '\0';
        return;
    }

    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##CountrySearch", LocalizedOr("#ServerBrowser_CountrySearch", "Find a country").c_str(),
                             m_szCountrySearch, sizeof(m_szCountrySearch));
    std::wstring search = nitro_utils::to_lower_copy(nitro_utils::utf8_to_wide(m_szCountrySearch));

    if (ImGui::Selectable(WithCount(all, tab.counts.all_countries).c_str(), m_CountryFilter.empty()))
    {
        m_CountryFilter.clear();
        OnFiltersChanged();
    }

    // the countries of the listed servers, and the picked one even when none of them is from there
    std::vector<std::pair<std::string, std::string>> countries;
    for (const auto& [code, count] : tab.counts.countries)
    {
        auto name = m_CountryNames.find(code);
        countries.emplace_back(code, name != m_CountryNames.end() ? name->second : code);
    }
    if (!m_CountryFilter.empty() && !tab.counts.countries.contains(m_CountryFilter))
        countries.emplace_back(m_CountryFilter, current);

    std::sort(countries.begin(), countries.end(), [](const auto& a, const auto& b)
    {
        return nitro_utils::to_lower_copy(nitro_utils::utf8_to_wide(a.second)) <
               nitro_utils::to_lower_copy(nitro_utils::utf8_to_wide(b.second));
    });

    for (const auto& [code, name] : countries)
    {
        std::wstring lowerName = nitro_utils::to_lower_copy(nitro_utils::utf8_to_wide(name));
        std::wstring lowerCode = nitro_utils::to_lower_copy(nitro_utils::utf8_to_wide(code));
        if (!search.empty() && lowerName.find(search) == std::wstring::npos && lowerCode != search)
            continue;

        if (int flag = FlagTexture(code.c_str()))
            DrawIcon(flag);
        else
            ImGui::Dummy(ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()));
        ImGui::SameLine();

        auto count = tab.counts.countries.find(code);
        std::string label = WithCount(name, count != tab.counts.countries.end() ? count->second : 0) + "##" + code;
        if (ImGui::Selectable(label.c_str(), code == m_CountryFilter))
        {
            m_CountryFilter = code;
            OnFiltersChanged();
        }
    }

    ImGui::EndCombo();
}

void CImGuiServerBrowser::DrawPingFilter(float width)
{
    std::string all = Localized("#ServerBrowser_All");
    std::string current = all;
    for (const PingLimit& limit : kPingLimits)
    {
        if (limit.ping == m_iPingFilter)
            current = Localized(limit.token);
    }
    std::string preview = Localized("#ServerBrowser_Latency") + ": " + current;

    ImGui::SetNextItemWidth(ComboWidth(preview, width));
    if (!ImGui::BeginCombo("##Ping", preview.c_str()))
        return;

    if (ImGui::Selectable(all.c_str(), m_iPingFilter == 0))
    {
        m_iPingFilter = 0;
        OnFiltersChanged();
    }

    for (const PingLimit& limit : kPingLimits)
    {
        if (ImGui::Selectable(Localized(limit.token).c_str(), limit.ping == m_iPingFilter))
        {
            m_iPingFilter = limit.ping;
            OnFiltersChanged();
        }
    }

    ImGui::EndCombo();
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
                ImGui::TextUnformatted(ServerBrowserText_ToVisualOrder(gs.GetName()).c_str());
                break;
            case kColumnMode:
                if (server.next_details.game_mode[0])
                    ImGui::TextUnformatted(GameModeText(server.next_details.game_mode).c_str());
                break;
            case kColumnPlayers:
                DrawPlayerCount(gs);
                break;
            case kColumnBots:
                if (gs.m_nBotPlayers > 0)
                    ImGui::Text("%d", gs.m_nBotPlayers);
                break;
            case kColumnMap:
                ImGui::TextUnformatted(gs.m_szMap);
                break;
            case kColumnPing:
                DrawPing(gs);
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

    if (ImGui::MenuItem(Localized("#ServerBrowser_ViewServerInfo").c_str()))
        OpenServerInfo(tab.servers.GetServer(tab.selected), tab.source);

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
        ImGui::TextUnformatted(ServerBrowserText_ToVisualOrder(m_PasswordServer.GetName()).c_str());
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

void CImGuiServerBrowser::DrawServerInfo()
{
    if (!m_pServerInfo)
        return;

    CServerInfoQuery& query = *m_pServerInfo;
    double now = system()->GetCurrentTime();

    // a new answer: once a slot is free the full notice goes, and auto-retry joins
    if (query.GetResponseCount() != m_iInfoResponsesSeen)
    {
        m_iInfoResponsesSeen = query.GetResponseCount();
        if (!query.IsNotResponding() && !IsServerFull(query.GetServer()))
        {
            m_bInfoFull = false;
            if (m_bAutoJoin)
            {
                m_bAutoJoin = false;
                surface()->PlaySound("servers/game_ready.wav");

                // joining may close the window and delete the query, so nothing of it is used after
                JoinServer(serveritem_t(true, -1, query.GetServer(), m_InfoDetails), m_InfoSource);
                if (m_pServerInfo.get() != &query)
                    return;
            }
        }
    }

    if (m_bAutoJoin && !query.IsBusy() && now >= m_flNextInfoRetry)
    {
        query.Refresh();
        m_flNextInfoRetry = now + kInfoRetryInterval;
    }

    if (m_bInfoAppearing)
    {
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowFocus();
        m_bInfoAppearing = false;
    }
    ImGui::SetNextWindowSize(ImVec2(600, 520), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(480, 380), ImVec2(FLT_MAX, FLT_MAX));

    bool open = true;
    std::string title = Localized("#ServerBrowser_GameInfoTitle") + "###ServerInfo";
    if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse))
    {
        const gameserveritem_t& gs = query.GetServer();
        DrawServerDetails(gs);

        if (query.IsNotResponding())
            ImGui::TextColored(kBadColor, "%s", Localized("#ServerBrowser_ServerNotResponding").c_str());
        else if (m_bAutoJoin)
            ImGui::TextColored(kFairColor, "%s", Localized("#ServerBrowser_JoinWhenSlotIsFree").c_str());
        else if (m_bInfoFull)
            ImGui::TextColored(kBadColor, "%s", Localized("#ServerBrowser_CouldNotConnectServerFull").c_str());

        // the buttons sit under a separator at the bottom, the players take the rest
        float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y + 1.0f;
        DrawPlayers(query, -footer);
        ImGui::Separator();

        bool join = ImGui::Button(Localized("#ServerBrowser_JoinGame").c_str());
        ImGui::SameLine();
        ImGui::BeginDisabled(query.IsBusy());
        if (ImGui::Button(Localized("#ServerBrowser_Refresh").c_str()))
            query.Refresh();
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Checkbox(Localized("#ServerBrowser_AutoRetry").c_str(), &m_bAutoJoin))
            m_flNextInfoRetry = 0.0;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", Localized("#ServerBrowser_JoinWhenSlotOpens").c_str());

        std::string closeLabel = Localized("#ServerBrowser_Close");
        float closeWidth = ImGui::CalcTextSize(closeLabel.c_str()).x + ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - closeWidth);
        if (ImGui::Button(closeLabel.c_str()))
            open = false;

        bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        if (focused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape))
            open = false;

        ImGui::End();

        // joining may delete the query, so it goes last
        if (join && open)
        {
            JoinServer(serveritem_t(true, -1, gs, m_InfoDetails), m_InfoSource);
            return;
        }
    }
    else
    {
        ImGui::End();
    }

    if (!open)
    {
        m_pServerInfo.reset();
        m_bAutoJoin = false;
    }
}

void CImGuiServerBrowser::DrawServerDetails(const gameserveritem_t& gs)
{
    if (int flag = FlagTexture(m_InfoDetails.country_code))
    {
        DrawIcon(flag);
        ImGui::SameLine();
    }
    ImGui::TextColored(kTitleColor, "%s", ServerBrowserText_ToVisualOrder(gs.GetName()).c_str());

    // the address copies itself on a click, so it needs no button of its own
    std::string address = gs.m_NetAdr.GetConnectionAddressString();
    ImGui::TextDisabled("%s", address.c_str());
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", LocalizedOr("#ServerBrowser_CopyAddress", "Copy address").c_str());
    if (ImGui::IsItemClicked())
        ImGui::SetClipboardText(address.c_str());

    ImGui::Separator();

    // two pairs of name and value a row keep the details short, leaving the room to the players
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp;
    if (!ImGui::BeginTable("Details", 4, flags))
        return;

    ImGui::TableSetupColumn("Name1", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Value1", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Name2", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Value2", ImGuiTableColumnFlags_WidthStretch);

    int cell = 0;
    auto name = [&cell](const char* token)
    {
        if (cell % 4 == 0)
            ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(cell % 4);
        ImGui::TextDisabled("%s", Localized(token).c_str());
        ImGui::TableSetColumnIndex(cell % 4 + 1);
        cell += 2;
    };

    name("#ServerBrowser_Map");
    ImGui::TextUnformatted(gs.m_szMap);

    name("#ServerBrowser_GameMode");
    ImGui::TextUnformatted(m_InfoDetails.game_mode[0] ? GameModeText(m_InfoDetails.game_mode).c_str() : "-");

    name("#ServerBrowser_Players");
    DrawPlayerCount(gs);
    if (gs.m_nBotPlayers > 0)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("+%d %s", gs.m_nBotPlayers, Localized("#ServerBrowser_Bots").c_str());
    }

    name("#ServerBrowser_Country");
    ImGui::TextUnformatted(m_InfoDetails.country_name[0] ? m_InfoDetails.country_name : m_InfoDetails.country_code[0] ? m_InfoDetails.country_code : "-");

    name("#ServerBrowser_Latency");
    DrawPing(gs);

    name("#ServerBrowser_Secure");
    if (gs.m_bSecure)
        DrawIcon(Texture("servers/icon_robotron"));
    else
        ImGui::TextDisabled("-");

    ImGui::EndTable();
    ImGui::Spacing();
}

void CImGuiServerBrowser::DrawPlayers(const CServerInfoQuery& query, float height)
{
    // on the field color, like the server list
    ImGui::BeginChild("Players", ImVec2(0, height));
    ImGuiTableFlags flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("PlayerList", 3, flags))
    {
        ImGui::EndChild();
        return;
    }

    enum { kPlayerName, kPlayerScore, kPlayerTime };
    float charWidth = ImGui::CalcTextSize("0").x;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(Localized("#ServerBrowser_PlayerName").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.0f, kPlayerName);
    ImGui::TableSetupColumn(Localized("#ServerBrowser_Score").c_str(), ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_DefaultSort, charWidth * 7, kPlayerScore);
    ImGui::TableSetupColumn(Localized("#ServerBrowser_Time").c_str(), ImGuiTableColumnFlags_PreferSortDescending, charWidth * 11, kPlayerTime);
    ImGui::TableHeadersRow();

    // a server has a few dozen players at most, so they're sorted on every frame
    const std::vector<CServerInfoQuery::Player>& players = query.GetPlayers();
    std::vector<const CServerInfoQuery::Player*> sorted;
    for (const auto& player : players)
        sorted.push_back(&player);

    if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsCount > 0)
    {
        const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
        bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
        std::stable_sort(sorted.begin(), sorted.end(), [&spec, ascending](const auto* a, const auto* b)
        {
            int order = 0;
            if (spec.ColumnUserID == kPlayerName)
                order = V_stricmp(a->name.c_str(), b->name.c_str());
            else if (spec.ColumnUserID == kPlayerScore)
                order = a->score - b->score;
            else
                order = a->seconds < b->seconds ? -1 : a->seconds > b->seconds;
            return ascending ? order < 0 : order > 0;
        });
    }

    for (const auto* player : sorted)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(kPlayerName);
        ImGui::TextUnformatted(ServerBrowserText_ToVisualOrder(player->name).c_str());
        ImGui::TableSetColumnIndex(kPlayerScore);
        ImGui::Text("%d", player->score);
        ImGui::TableSetColumnIndex(kPlayerTime);
        ImGui::TextUnformatted(FormatPlayedTime(player->seconds).c_str());
    }

    ImGui::EndTable();
    ImGui::EndChild();
}
