#include "ImGuiPlayerList.h"
#include "GameUi.h"
#include "ImGuiForm.h"

#include <vgui/ILocalize.h>

// cdll_int.h, for hud_player_info_t (whether a row is us), needs the SDK's types without including them, like engine_mini's hlsdk.h
#include <extdll.h>
#include <pm_defs.h>
#include <usercmd.h>
#include <kbutton.h>
#include <ref_params.h>
#include <cl_entity.h>
#include <entity_state.h>
#include <weaponinfo.h>
#include <r_efx.h>
#include <r_studioint.h>
#include <cdll_int.h>

#include <cvardef.h>
#include <tier1/strtools.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cfloat>
#include <vector>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace ImGuiForm;

namespace
{
    cvar_t* g_pLegacyCvar = nullptr;

    struct Player
    {
        int index;
        std::string name;
        bool bot;
        bool self;
    };

    std::vector<Player> ReadPlayers()
    {
        std::vector<Player> players;
        for (int i = 1; i <= engine->GetMaxClients(); i++)
        {
            const char* name = engine->PlayerInfo_ValueForKey(i, "name");
            if (!name || !name[0])
                continue;

            hud_player_info_t info{};
            engine->pfnGetPlayerInfo(i, &info);

            Player player;
            player.index = i;
            player.name = name;
            player.self = info.thisplayer != 0;

            const char* bot = engine->PlayerInfo_ValueForKey(i, "*bot");
            player.bot = bot && !V_stricmp(bot, "1");

            players.push_back(std::move(player));
        }
        return players;
    }

    std::string ToLower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)tolower(c); });
        return text;
    }
}

CImGuiPlayerList::CImGuiPlayerList() : BaseClass("playerlist_layout.ini")
{
    SetAppearance(ImGuiAppearance::Element::PlayerList);
    SetVisible(false);
}

void CImGuiPlayerList::RegisterCvars()
{
    g_pLegacyCvar = engine->pfnRegisterVariable("plist_legacy", "0", FCVAR_ARCHIVE);
}

bool CImGuiPlayerList::UseLegacyDialog()
{
    return g_pLegacyCvar && g_pLegacyCvar->value != 0.0f;
}

void CImGuiPlayerList::Activate()
{
    if (!IsVisible())
        m_szSearch[0] = '\0';

    SetVisible(true);
    MoveToFront();
    RequestFocus();
    ResetInput();
    m_bFocusWindow = true;
}

void CImGuiPlayerList::Close()
{
    SetVisible(false);
}

std::string CImGuiPlayerList::Title() const
{
    const char* hostName = GameClientExports() ? GameClientExports()->GetServerHostName() : nullptr;
    const wchar_t* format = g_pVGuiLocalize->Find("#GameUI_PlayerListDialogTitle");
    if (!hostName || !hostName[0] || !format)
        return Localized("#GameUI_CurrentPlayers", "Current players");

    wchar_t host[128], title[256];
    V_UTF8ToUnicode(hostName, host, sizeof(host));
    g_pVGuiLocalize->ConstructString(title, sizeof(title), const_cast<wchar_t*>(format), 1, host);

    char utf8[512];
    V_UnicodeToUTF8(title, utf8, sizeof(utf8));
    return utf8;
}

void CImGuiPlayerList::DrawImGui()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(OnScreen(ImVec2(480, 480)), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(OnScreen(ImVec2(360, 260)), ImVec2(FLT_MAX, FLT_MAX));

    // without the border a checkbox is a blank square
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    if (m_bFocusWindow)
    {
        ImGui::SetNextWindowFocus();
        m_bFocusWindow = false;
    }

    bool open = true;
    std::string title = Title() + "###PlayerList";
    if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse))
    {
        std::vector<Player> players = ReadPlayers();
        const ImGuiStyle& style = ImGui::GetStyle();

        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputTextWithHint("##Search", Localized("#GameUI_PlayerListSearch", "Search players").c_str(), m_szSearch, sizeof(m_szSearch));
        ImGui::Dummy(ImVec2(0, 2));

        std::string search = ToLower(m_szSearch);
        int others = 0;
        for (const Player& player : players)
            others += !player.self;

        float footer = ImGui::GetFrameHeight() + style.ItemSpacing.y * 2.0f;
        ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY | ImGuiTableFlags_PadOuterX;
        if (ImGui::BeginTable("Players", 2, tableFlags, ImVec2(0, -footer)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn(Localized("#GameUI_PlayerName").c_str(), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(Localized("#GameUI_PlayerListMuted", "Muted").c_str(), ImGuiTableColumnFlags_WidthFixed, 96.0f);
            ImGui::TableHeadersRow();

            for (const Player& player : players)
            {
                if (!search.empty() && ToLower(player.name).find(search) == std::string::npos)
                    continue;

                ImGui::PushID(player.index);
                ImGui::TableNextRow(0, ImGui::GetFrameHeight());

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                if (player.self)
                    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", player.name.c_str());
                else
                    ImGui::TextUnformatted(player.name.c_str());
                if (player.self || player.bot)
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", player.bot ? "BOT" : Localized("#GameUI_PlayerListYou", "you").c_str());
                }

                ImGui::TableNextColumn();
                if (!player.bot && !player.self && GameClientExports())
                {
                    bool muted = GameClientExports()->IsPlayerGameVoiceMuted(player.index);
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight()) * 0.5f);
                    if (ImGui::Checkbox("##Muted", &muted))
                    {
                        if (muted)
                            GameClientExports()->MutePlayerGameVoice(player.index);
                        else
                            GameClientExports()->UnmutePlayerGameVoice(player.index);
                    }
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        if (others == 0)
        {
            std::string empty = Localized("#GameUI_NoOtherPlayersInGame");
            ImVec2 size = ImGui::CalcTextSize(empty.c_str());
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddText(ImVec2((min.x + max.x - size.x) * 0.5f, (min.y + max.y - size.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), empty.c_str());
        }

        ImGui::Dummy(ImVec2(0, style.ItemSpacing.y));

        std::string count = Localized("#GameUI_PlayerListCount", "Players: %d");
        size_t at = count.find("%d");
        if (at != std::string::npos)
            count.replace(at, 2, std::to_string(players.size()));
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", count.c_str());

        std::string close = Localized("#GameUI_Close");
        float buttonWidth = std::max(96.0f, ImGui::CalcTextSize(close.c_str()).x + style.FramePadding.x * 2.0f);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - buttonWidth);
        if (ImGui::Button(close.c_str(), ImVec2(buttonWidth, 0)))
            open = false;

        // in a level, Escape belongs to the game menu
        bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        if (focused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape) && !GameUI().IsInLevel())
            open = false;
    }
    ImGui::End();

    ImGui::PopStyleVar();

    if (!open)
        Close();
}
