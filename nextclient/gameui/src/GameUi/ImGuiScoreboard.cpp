#include "ImGuiScoreboard.h"
#include "GameUi.h"
#include "ImGuiForm.h"

#include <vgui/ISurfaceNext.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Controls.h>

#include <cvardef.h>
#include <tier1/strtools.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <steam/steam_api.h>

#ifdef _WIN32
#include <Windows.h>
#endif
#include <GL/gl.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace ImGuiForm;

namespace
{
    cvar_t* g_pLegacyCvar = nullptr;

    // the score in big digits between the team names
    constexpr float kTitleScale = 2.6f;

    const ImVec4 kCTColor(0.45f, 0.68f, 1.0f, 1.0f);
    const ImVec4 kTColor(1.0f, 0.5f, 0.38f, 1.0f);
    const ImVec4 kBombColor(1.0f, 0.62f, 0.2f, 1.0f);
    const ImVec4 kVipColor(1.0f, 0.85f, 0.3f, 1.0f);

    // seconds on a clock that only goes forward; not ISystem::GetCurrentTime, which windows.h renames
    // with a macro of its own wherever it comes in before vgui/ISystem.h
    double Now()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    std::vector<const ScoreboardPlayer*> PlayersOf(const ScoreboardState& state, ScoreboardTeam team)
    {
        std::vector<const ScoreboardPlayer*> players;
        for (int i = 0; i < state.player_count && i < (int)std::size(state.players); i++)
        {
            if (state.players[i].team == team)
                players.push_back(&state.players[i]);
        }

        // the most kills first, then the fewest deaths, like the client's scoreboard
        std::stable_sort(players.begin(), players.end(), [](const ScoreboardPlayer* a, const ScoreboardPlayer* b)
        {
            if (a->frags != b->frags)
                return a->frags > b->frags;
            return a->deaths < b->deaths;
        });
        return players;
    }

    void TextRight(const char* text)
    {
        float width = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - width));
        ImGui::TextUnformatted(text);
    }

    // a small rounded tag with a word in it, for the dead, the bomb carrier and the VIP
    void Badge(const std::string& text, const ImVec4& color)
    {
        ImVec2 size = ImGui::CalcTextSize(text.c_str());
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 padding(5.0f, 1.0f);
        ImVec2 end(pos.x + size.x + padding.x * 2.0f, pos.y + size.y + padding.y * 2.0f);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec4 fill = color;
        fill.w *= 0.22f;
        drawList->AddRectFilled(pos, end, ImGui::GetColorU32(fill), 3.0f);
        drawList->AddText(ImVec2(pos.x + padding.x, pos.y + padding.y), ImGui::GetColorU32(color), text.c_str());
        ImGui::Dummy(ImVec2(end.x - pos.x, end.y - pos.y));
    }

    std::string Format(const std::string& format, int value)
    {
        std::string text = format;
        size_t at = text.find("%d");
        if (at != std::string::npos)
            text.replace(at, 2, std::to_string(value));
        return text;
    }
}

CImGuiScoreboard::CImGuiScoreboard() : BaseClass(nullptr, kTitleScale)
{
    SetAppearance(ImGuiAppearance::Element::Scoreboard);
    SetVisible(false);
    // a look at the scores mustn't take the keys or the mouse from the game
    SetKeyBoardInputEnabled(false);
    SetMouseInputEnabled(false);
}

void CImGuiScoreboard::PreparePreview()
{
    struct Sample
    {
        const char* name;
        ScoreboardTeam team;
        int frags;
        int deaths;
        int ping;
        bool dead;
        bool bomb;
    };

    static const Sample kSamples[] = {
        { "Kotyara", ScoreboardTeam::CounterTerrorist, 24, 3, 38, false, false },
        { "Inford", ScoreboardTeam::CounterTerrorist, 18, 7, 52, false, false },
        { "lirikaZz", ScoreboardTeam::CounterTerrorist, 11, 9, 64, true, false },
        { "dimas", ScoreboardTeam::Terrorist, 20, 5, 41, false, true },
        { "Oleg", ScoreboardTeam::Terrorist, 12, 8, 77, false, false },
        { "Ivan", ScoreboardTeam::Terrorist, 4, 12, 95, true, false },
        { "spectator", ScoreboardTeam::Spectator, 0, 0, 20, false, false },
    };

    ScoreboardState state{};
    state.ct_score = 5;
    state.terrorist_score = 3;
    for (const Sample& sample : kSamples)
    {
        ScoreboardPlayer& player = state.players[state.player_count];
        player.index = ++state.player_count;
        V_strncpy(player.name, sample.name, sizeof(player.name));
        player.team = sample.team;
        player.frags = sample.frags;
        player.deaths = sample.deaths;
        player.ping = sample.ping;
        player.dead = sample.dead;
        player.bomb = sample.bomb;
        player.self = player.index == 1;
    }
    SetState(state);
}

void CImGuiScoreboard::SetState(const ScoreboardState& state)
{
    m_State = state;

    double now = Now();
    for (int i = 0; i < m_State.player_count && i < (int)std::size(m_State.players); i++)
    {
        ScoreboardPlayer& player = m_State.players[i];
        ShownPing& shown = m_ShownPings[player.index];
        if (now - shown.time >= 1.0 || now < shown.time)
        {
            shown.ping = player.ping;
            shown.time = now;
        }
        player.ping = shown.ping;
    }
}

void CImGuiScoreboard::DrawImGui()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float width = std::clamp(viewport->Size.x * 0.78f, 760.0f, 1280.0f);
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 18.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kRounding * 1.5f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("###Scoreboard", nullptr, flags))
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        float content = ImGui::GetContentRegionAvail().x;
        float left = ImGui::GetCursorPosX();

        // the server and the map, centered
        const char* host = GameClientExports() ? GameClientExports()->GetServerHostName() : nullptr;
        std::string server = host && host[0] ? host : "";
        std::string map = engine->pfnGetLevelName();
        size_t slash = map.find_last_of('/');
        if (slash != std::string::npos)
            map.erase(0, slash + 1);
        if (map.size() > 4 && map.compare(map.size() - 4, 4, ".bsp") == 0)
            map.erase(map.size() - 4);

        if (!server.empty())
        {
            ImGui::PushFont(HeadingFont());
            ImGui::SetCursorPosX(left + std::max(0.0f, (content - ImGui::CalcTextSize(server.c_str()).x) * 0.5f));
            ImGui::TextUnformatted(server.c_str());
            ImGui::PopFont();
        }
        ImGui::SetCursorPosX(left + std::max(0.0f, (content - ImGui::CalcTextSize(map.c_str()).x) * 0.5f));
        ImGui::TextDisabled("%s", map.c_str());

        // the team names at the sides and the rounds won between them in big digits
        auto teamInfo = [this](ScoreboardTeam team)
        {
            std::vector<const ScoreboardPlayer*> players = PlayersOf(m_State, team);
            int ping = 0, counted = 0;
            for (const ScoreboardPlayer* player : players)
            {
                if (!player->bot)
                {
                    ping += player->ping;
                    counted++;
                }
            }

            std::string text = Format(Localized("#GameUI_ScoreboardPlayers", "%d players"), (int)players.size());
            if (counted > 0)
                text += "  ·  " + Format(Localized("#GameUI_ScoreboardAvgPing", "ping %d"), ping / counted);
            return text;
        };

        ImGui::Dummy(ImVec2(0, 4));
        float rowTop = ImGui::GetCursorPosY();

        std::string ctName = Localized("#Cstrike_ScoreBoard_CT", "Counter-Terrorists");
        std::string tName = Localized("#Cstrike_ScoreBoard_Ter", "Terrorists");
        std::string ctInfo = teamInfo(ScoreboardTeam::CounterTerrorist);
        std::string tInfo = teamInfo(ScoreboardTeam::Terrorist);

        char ctScore[16], tScore[16];
        snprintf(ctScore, sizeof(ctScore), "%d", m_State.ct_score);
        snprintf(tScore, sizeof(tScore), "%d", m_State.terrorist_score);

        ImGui::PushFont(TitleFont());
        float digits = ImGui::GetFontSize();
        float colon = ImGui::CalcTextSize(":").x;
        float ctWidth = ImGui::CalcTextSize(ctScore).x;
        float center = left + content * 0.5f;
        float gap = digits * 0.5f;
        ImGui::SetCursorPos(ImVec2(center - colon * 0.5f - gap - ctWidth, rowTop));
        ImGui::TextColored(kCTColor, "%s", ctScore);
        ImGui::SetCursorPos(ImVec2(center - colon * 0.5f, rowTop));
        ImGui::TextDisabled(":");
        ImGui::SetCursorPos(ImVec2(center + colon * 0.5f + gap, rowTop));
        ImGui::TextColored(kTColor, "%s", tScore);
        ImGui::PopFont();
        float rowBottom = ImGui::GetCursorPosY();

        float labels = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetFontSize() * 1.4f;
        float labelTop = rowTop + std::max(0.0f, (rowBottom - rowTop - labels) * 0.5f);

        ImGui::SetCursorPos(ImVec2(left, labelTop));
        ImGui::PushFont(HeadingFont());
        ImGui::TextColored(kCTColor, "%s", ctName.c_str());
        ImGui::PopFont();
        ImGui::SetCursorPosX(left);
        ImGui::TextDisabled("%s", ctInfo.c_str());

        ImGui::PushFont(HeadingFont());
        ImGui::SetCursorPos(ImVec2(left + content - ImGui::CalcTextSize(tName.c_str()).x, labelTop));
        ImGui::TextColored(kTColor, "%s", tName.c_str());
        ImGui::PopFont();
        ImGui::SetCursorPosX(left + content - ImGui::CalcTextSize(tInfo.c_str()).x);
        ImGui::TextDisabled("%s", tInfo.c_str());

        ImGui::SetCursorPos(ImVec2(left, std::max(rowBottom, ImGui::GetCursorPosY())));
        ImGui::Dummy(ImVec2(0, 6));

        // the two teams side by side, the counter-terrorists on the left
        float half = (content - style.ItemSpacing.x * 3.0f) * 0.5f;
        ImGui::BeginGroup();
        DrawTeam(ScoreboardTeam::CounterTerrorist, "CT", half);
        ImGui::EndGroup();
        ImGui::SameLine(0.0f, style.ItemSpacing.x * 3.0f);
        ImGui::BeginGroup();
        DrawTeam(ScoreboardTeam::Terrorist, "T", half);
        ImGui::EndGroup();

        DrawSpectators();
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
}

void CImGuiScoreboard::DrawTeam(ScoreboardTeam team, const char* id, float width)
{
    std::vector<const ScoreboardPlayer*> players = PlayersOf(m_State, team);
    const ImVec4& teamColor = team == ScoreboardTeam::CounterTerrorist ? kCTColor : kTColor;

    // a thin bar in the team's color over its table
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + 3.0f), ImGui::GetColorU32(teamColor), 1.5f);
    ImGui::Dummy(ImVec2(width, 3.0f));

    // the table sits on the dark panel the console and the browser keep their lists on; a table can't
    // go inside channels like the cards use, so the panel takes the table's height from the last frame
    float& lastHeight = team == ScoreboardTeam::CounterTerrorist ? m_flTableHeight[0] : m_flTableHeight[1];
    ImVec2 tableTop = ImGui::GetCursorScreenPos();
    if (lastHeight > 0.0f)
    {
        drawList->AddRectFilled(tableTop, ImVec2(tableTop.x + width, tableTop.y + lastHeight), ImGui::GetColorU32(ImGuiCol_ChildBg), kRounding, ImDrawFlags_RoundCornersBottom);
        drawList->AddRect(tableTop, ImVec2(tableTop.x + width, tableTop.y + lastHeight), ImGui::GetColorU32(ImGuiCol_Border), kRounding, ImDrawFlags_RoundCornersBottom);
    }

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_PadOuterX | ImGuiTableFlags_NoSavedSettings;
    if (!ImGui::BeginTable(id, 6, flags, ImVec2(width, 0.0f)))
        return;

    float digit = ImGui::CalcTextSize("0").x;
    ImGui::TableSetupColumn(Localized("#GameUI_ScoreboardName", "Name").c_str(), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, digit * 9.0f);
    ImGui::TableSetupColumn(Localized("#GameUI_ScoreboardKills", "K").c_str(), ImGuiTableColumnFlags_WidthFixed, digit * 4.0f);
    ImGui::TableSetupColumn(Localized("#GameUI_ScoreboardDeaths", "D").c_str(), ImGuiTableColumnFlags_WidthFixed, digit * 4.0f);
    ImGui::TableSetupColumn(Localized("#GameUI_ScoreboardKD", "K/D").c_str(), ImGuiTableColumnFlags_WidthFixed, digit * 5.0f);
    ImGui::TableSetupColumn(Localized("#GameUI_ScoreboardPing", "Ping").c_str(), ImGuiTableColumnFlags_WidthFixed, digit * 5.0f);

    // the numbers' headers sit over their right-aligned numbers
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    for (int column = 0; column < 6; column++)
    {
        ImGui::TableSetColumnIndex(column);
        const char* name = ImGui::TableGetColumnName(column);
        if (column >= 2)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            TextRight(name);
            ImGui::PopStyleColor();
        }
        else
            ImGui::TextDisabled("%s", name);
    }

    for (const ScoreboardPlayer* player : players)
    {
        ImGui::TableNextRow(0, ImGui::GetFrameHeight());
        if (player->self)
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ImGui::GetColorU32(WithAlpha(ImGuiCol_CheckMark, 0.14f)));

        // the dead are dimmed for the rest of the round
        if (player->dead)
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.55f);

        ImGui::TableNextColumn();
        if (player->self)
        {
            // a bar in the accent color down the row's left edge marks your own line
            ImVec2 rowTop = ImGui::GetCursorScreenPos();
            float left = ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), 0).Min.x;
            drawList->AddRectFilled(ImVec2(left, rowTop.y), ImVec2(left + 3.0f, rowTop.y + ImGui::GetFrameHeight()), ImGui::GetColorU32(ImGuiCol_CheckMark));
        }
        float avatar = ImGui::GetFrameHeight() - 4.0f;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
        DrawAvatar(*player, avatar);
        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
        ImGui::AlignTextToFramePadding();
        if (player->self)
        {
            // the accent itself is too dark to read on its own tint
            ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
            ImVec4 bright(accent.x + (1.0f - accent.x) * 0.55f, accent.y + (1.0f - accent.y) * 0.55f, accent.z + (1.0f - accent.z) * 0.55f, 1.0f);
            ImGui::TextColored(bright, "%s", player->name);
        }
        else
            ImGui::TextUnformatted(player->name);

        ImGui::TableNextColumn();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetFrameHeight() - ImGui::GetTextLineHeight()) * 0.5f - 1.0f);
        if (player->dead)
            Badge(Localized("#Cstrike_DEAD", "Dead"), ImGui::GetStyleColorVec4(ImGuiCol_Text));
        else if (player->bomb)
            Badge("C4", kBombColor);
        else if (player->vip)
            Badge(Localized("#Cstrike_VIP", "VIP"), kVipColor);

        char text[32];
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        snprintf(text, sizeof(text), "%d", player->frags);
        TextRight(text);

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        snprintf(text, sizeof(text), "%d", player->deaths);
        TextRight(text);

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        snprintf(text, sizeof(text), "%.2f", player->deaths > 0 ? (float)player->frags / player->deaths : (float)player->frags);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        TextRight(text);
        ImGui::PopStyleColor();

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        if (player->bot)
            V_strcpy_safe(text, "BOT");
        else
            snprintf(text, sizeof(text), "%d", player->ping);
        TextRight(text);

        if (player->dead)
            ImGui::PopStyleVar();
    }

    ImGui::EndTable();
    lastHeight = ImGui::GetItemRectSize().y;
}

unsigned int CImGuiScoreboard::AvatarTexture(uint64_t steamId)
{
    // only individual accounts have avatars: universe 1, account type 1 in the top 12 bits
    if ((steamId >> 52) != 0x011)
        return 0;

    Avatar& avatar = m_Avatars[steamId];
    if (avatar.texture)
        return avatar.texture;

    double now = Now();
    if (now < avatar.nextTry || !SteamFriends() || !SteamUtils())
        return 0;
    avatar.nextTry = now + 1.0;

    // 0 when Steam doesn't know the player yet, which it learns on request; -1 while it's loading
    CSteamID id(steamId);
    int image = SteamFriends()->GetSmallFriendAvatar(id);
    if (image == 0)
    {
        if (!avatar.requested)
            SteamFriends()->RequestUserInformation(id, false);
        avatar.requested = true;
        return 0;
    }
    if (image < 0)
        return 0;

    uint32 width, height;
    if (!SteamUtils()->GetImageSize(image, &width, &height) || !width || !height)
        return 0;

    std::vector<uint8> rgba(width * height * 4);
    if (!SteamUtils()->GetImageRGBA(image, rgba.data(), (int)rgba.size()))
        return 0;

    // a texture name from the engine's counter, like the font atlas's
    avatar.texture = vgui2::surface()->CreateNewTextureID();

    GLint lastTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);
    glBindTexture(GL_TEXTURE_2D, avatar.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, lastTexture);

    return avatar.texture;
}

void CImGuiScoreboard::DrawAvatar(const ScoreboardPlayer& player, float size)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 end(pos.x + size, pos.y + size);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    if (unsigned int texture = player.steam_id ? AvatarTexture(player.steam_id) : 0)
    {
        drawList->AddImageRounded(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture)), pos, end,
            ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1, 1, 1, ImGui::GetStyle().Alpha)), 3.0f);
    }
    else
    {
        // the name's first letter, a whole UTF-8 character
        const unsigned char* name = reinterpret_cast<const unsigned char*>(player.name);
        int length = name[0] < 0x80 ? 1 : name[0] < 0xE0 ? 2 : name[0] < 0xF0 ? 3 : 4;
        std::string initial(player.name, strnlen(player.name, length));

        drawList->AddRectFilled(pos, end, ImGui::GetColorU32(WithAlpha(ImGuiCol_Text, 0.12f)), 3.0f);
        ImVec2 text = ImGui::CalcTextSize(initial.c_str());
        drawList->AddText(ImVec2(pos.x + (size - text.x) * 0.5f, pos.y + (size - text.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), initial.c_str());
    }

    ImGui::Dummy(ImVec2(size, size));
}

void CImGuiScoreboard::DrawSpectators()
{
    std::vector<const ScoreboardPlayer*> spectators;
    for (int i = 0; i < m_State.player_count && i < (int)std::size(m_State.players); i++)
    {
        const ScoreboardPlayer& player = m_State.players[i];
        if (player.team == ScoreboardTeam::Spectator || player.team == ScoreboardTeam::Unassigned)
            spectators.push_back(&player);
    }

    if (spectators.empty())
        return;

    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::Dummy(ImVec2(0, 6));
    std::string title = Localized("#GameUI_ScoreboardSpectators", "Spectators");
    ImGui::TextDisabled("%s (%d)", title.c_str(), (int)spectators.size());

    // each spectator as a chip with the avatar and the name, as many to a line as fit
    float avatar = ImGui::GetFrameHeight() - 4.0f;
    float padding = 6.0f;
    float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    bool first = true;
    for (const ScoreboardPlayer* player : spectators)
    {
        float chipWidth = padding + avatar + style.ItemInnerSpacing.x + ImGui::CalcTextSize(player->name).x + padding;
        if (!first)
        {
            ImGui::SameLine(0.0f, style.ItemSpacing.x);
            if (ImGui::GetCursorScreenPos().x + chipWidth > right)
                ImGui::NewLine();
        }
        first = false;

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 end(pos.x + chipWidth, pos.y + ImGui::GetFrameHeight());
        drawList->AddRectFilled(pos, end, ImGui::GetColorU32(player->self ? WithAlpha(ImGuiCol_CheckMark, 0.25f) : ImGui::GetStyleColorVec4(ImGuiCol_ChildBg)), kRounding);
        drawList->AddRect(pos, end, ImGui::GetColorU32(ImGuiCol_Border), kRounding);

        ImGui::BeginGroup();
        ImGui::SetCursorScreenPos(ImVec2(pos.x + padding, pos.y + 2.0f));
        DrawAvatar(*player, avatar);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, pos.y + (ImGui::GetFrameHeight() - ImGui::GetTextLineHeight()) * 0.5f));
        ImGui::TextUnformatted(player->name);
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(chipWidth, ImGui::GetFrameHeight()));
        ImGui::EndGroup();
    }
}

static CScoreboardNext g_ScoreboardNext;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CScoreboardNext, IScoreboardNext, SCOREBOARD_NEXT_INTERFACE_VERSION, g_ScoreboardNext);

void CScoreboardNext::RegisterCvars()
{
    g_pLegacyCvar = engine->pfnRegisterVariable("scoreboard_legacy", "0", FCVAR_ARCHIVE);
}

bool CScoreboardNext::IsEnabled()
{
    return g_pLegacyCvar && g_pLegacyCvar->value == 0.0f;
}

void CScoreboardNext::Show(const ScoreboardState& state)
{
    if (!m_hPanel.Get())
    {
        // under the root panel: the menu, and everything under it, is hidden while playing
        m_hPanel = vgui2::SETUP_PANEL(new CImGuiScoreboard());
        m_hPanel->SetParent(vgui2::surface()->GetEmbeddedPanel());
    }

    m_hPanel->SetState(state);
    if (!m_hPanel->IsVisible())
    {
        m_hPanel->SetVisible(true);
        m_hPanel->MoveToFront();
    }
}

void CScoreboardNext::Hide()
{
    if (m_hPanel.Get())
        m_hPanel->SetVisible(false);
}
