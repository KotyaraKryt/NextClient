#include "ImGuiCreateServer.h"
#include "CreateMultiPlayerGameDialog/CreateMultiplayerGameGameplayPage.h"
#include "GameUi.h"
#include "ModInfo.h"
#include "ScriptObject.h"

#include <FileSystem.h>
#include <KeyValues.h>

#include <cvardef.h>
#include <tier1/strtools.h>

#include <imgui/imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstdlib>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace ImGuiForm;

namespace
{
    cvar_t* g_pLegacyCvar = nullptr;

    constexpr float kMapListWidth = 240.0f;
    constexpr float kPagePadding = 18.0f;

    // the rules the Server card shows; settings.scr's other options go in Rules
    const char* const kServerRules[] = { "hostname", "sv_password", "maxplayers" };

    bool IsServerRule(const char* cvar)
    {
        for (const char* rule : kServerRules)
        {
            if (!V_stricmp(rule, cvar))
                return true;
        }
        return false;
    }

    struct BotChoice
    {
        const char* token;
        const char* value;
    };

    const BotChoice kBotTeams[] = {
        { "#Cstrike_Random", "any" },
        { "#Cstrike_ScoreBoard_CT", "ct" },
        { "#Cstrike_ScoreBoard_Ter", "t" },
    };

    const BotChoice kBotChatter[] = {
        { "#Cstrike_Bot_Chatter_Normal", "normal" },
        { "#Cstrike_Bot_Chatter_Minimal", "minimal" },
        { "#Cstrike_Bot_Chatter_Radio", "radio" },
        { "#Cstrike_Bot_Chatter_Off", "off" },
    };

    const char* const kBotDifficulties[] = {
        "#Cstrike_Bot_Difficulty0",
        "#Cstrike_Bot_Difficulty1",
        "#Cstrike_Bot_Difficulty2",
        "#Cstrike_Bot_Difficulty3",
    };

    struct BotWeapon
    {
        const char* token;
        const char* key;
    };

    const BotWeapon kBotWeapons[] = {
        { "#CStrike_Bot_UsePistols", "bot_allow_pistols" },
        { "#CStrike_Bot_UseShotguns", "bot_allow_shotguns" },
        { "#CStrike_Bot_UseSub", "bot_allow_sub_machine_guns" },
        { "#CStrike_Bot_UseRifles", "bot_allow_rifles" },
        { "#CStrike_Bot_UseMachineGuns", "bot_allow_machine_guns" },
        { "#CStrike_Bot_UseSniper", "bot_allow_snipers" },
        { "#CStrike_Bot_UseGrenades", "bot_allow_grenades" },
        { "#CStrike_Bot_UseShield", "bot_allow_shield" },
    };

    // what the old dialog wrote when CSBotConfig.vdf was missing
    const std::pair<const char*, const char*> kBotDefaults[] = {
        { "bot_difficulty", "0" },
        { "bot_join_after_player", "1" },
        { "bot_allow_rogues", "1" },
        { "bot_allow_pistols", "1" },
        { "bot_allow_shotguns", "1" },
        { "bot_allow_sub_machine_guns", "1" },
        { "bot_allow_machine_guns", "1" },
        { "bot_allow_rifles", "1" },
        { "bot_allow_snipers", "1" },
        { "bot_allow_grenades", "1" },
        { "bot_allow_shield", "1" },
        { "bot_join_team", "any" },
        { "bot_quota", "9" },
        { "bot_defer_to_human", "0" },
        { "bot_chatter", "normal" },
        { "bot_prefix", "" },
    };

    std::string ToLower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)tolower(c); });
        return text;
    }
}

CImGuiCreateServer::CImGuiCreateServer() : BaseClass("createserver_layout.ini")
{
    SetAppearance(ImGuiAppearance::Element::CreateServer);
    SetVisible(false);
}

CImGuiCreateServer::~CImGuiCreateServer()
{
    if (m_pBotConfig)
        m_pBotConfig->deleteThis();
}

void CImGuiCreateServer::RegisterCvars()
{
    g_pLegacyCvar = engine->pfnRegisterVariable("newgame_legacy", "0", FCVAR_ARCHIVE);
}

bool CImGuiCreateServer::UseLegacyDialog()
{
    return g_pLegacyCvar && g_pLegacyCvar->value != 0.0f;
}

void CImGuiCreateServer::PreparePreview()
{
    LoadMaps();
    LoadRules();
    LoadBotConfig();
}

void CImGuiCreateServer::Activate()
{
    // like the old dialog, every opening starts from the files, and finds maps added since
    if (!IsVisible())
    {
        LoadMaps();
        LoadRules();
        LoadBotConfig();
        m_szMapSearch[0] = '\0';
        m_bScrollToMap = true;
    }

    SetVisible(true);
    MoveToFront();
    RequestFocus();
    ResetInput();
    m_bFocusWindow = true;
}

void CImGuiCreateServer::Close()
{
    SetVisible(false);
}

void CImGuiCreateServer::LoadMaps()
{
    m_Maps.clear();

    // Half-Life's maps are in the base directory, which has no path ID of its own
    bool halfLife = !V_stricmp(ModInfo().GetGameDescription(), "Half-Life");
    const char* pathID = halfLife ? nullptr : "GAME";
    LoadMapsFrom(pathID);

    // a mod with a fallback_dir in gameinfo.txt can run that game's maps too
    if (pathID && ModInfo().GetFallbackDir()[0])
        LoadMapsFrom("GAME_FALLBACK");

    LoadMapsFrom("GAMEDOWNLOAD");

    std::sort(m_Maps.begin(), m_Maps.end(), [](const std::string& a, const std::string& b) { return V_stricmp(a.c_str(), b.c_str()) < 0; });
    m_Maps.erase(std::unique(m_Maps.begin(), m_Maps.end(), [](const std::string& a, const std::string& b) { return !V_stricmp(a.c_str(), b.c_str()); }), m_Maps.end());
}

void CImGuiCreateServer::LoadMapsFrom(const char* pathID)
{
    const char* filter = ModInfo().GetMPFilter();
    if (filter && !filter[0])
        filter = nullptr;

    const char* game = ModInfo().GetGameDescription();

    FileFindHandle_t handle = 0;
    for (const char* file = g_pFullFileSystem->FindFirst("maps/*.bsp", &handle, pathID); file; file = g_pFullFileSystem->FindNext(handle))
    {
        std::string name = file;
        size_t slash = name.find_last_of("/\\");
        if (slash != std::string::npos)
            name.erase(0, slash + 1);
        if (name.size() > 4 && !V_stricmp(name.c_str() + name.size() - 4, ".bsp"))
            name.erase(name.size() - 4);
        if (name.empty() || name[0] == '.')
            continue;

        // the single player maps, which the old dialog left out the same way
        if (!V_stricmp(game, "Half-Life") && name.size() > 2 && (name[0] == 'c' || name[0] == 't') && name[1] >= '0' && name[1] <= '5' && name[2] == 'a')
            continue;
        if (!V_stricmp(game, "Opposing Force") && name.size() > 1 && name[0] == 'o' && name[1] == 'f')
            continue;
        if (filter && strstr(name.c_str(), filter))
            continue;

        m_Maps.push_back(name);
    }
    g_pFullFileSystem->FindClose(handle);
}

void CImGuiCreateServer::LoadRules()
{
    m_Rules.clear();

    // settings.scr keeps the values chosen last as each option's default
    m_pRules = std::make_unique<CServerDescription>(nullptr);
    m_pRules->InitFromFile("settings.scr");

    for (CScriptObject* option = m_pRules->pObjList; option; option = option->pNext)
    {
        std::string value = option->defValue;
        if (option->type == O_LIST)
        {
            int index = atoi(option->defValue);
            CScriptListItem* item = option->pListItems;
            for (int i = 0; item && i < index; i++)
                item = item->pNext;
            if (item)
                value = item->szValue;
        }
        m_Rules[option->cvarname] = value;
    }
}

void CImGuiCreateServer::LoadBotConfig()
{
    if (m_pBotConfig)
        m_pBotConfig->deleteThis();

    m_pBotConfig = new KeyValues("CSBotConfig");
    m_pBotConfig->LoadFromFile(g_pFullFileSystem, "CSBotConfig.vdf");

    for (const auto& [key, value] : kBotDefaults)
    {
        if (!m_pBotConfig->FindKey(key))
            m_pBotConfig->SetString(key, value);
    }

    // -1 is the old dialog's "no bots"; the difficulty is kept for when they come back
    int difficulty = m_pBotConfig->GetInt("bot_difficulty");
    m_bBotsEnabled = difficulty >= 0;
    m_iBotDifficulty = std::clamp(difficulty, 0, 3);

    const char* map = m_pBotConfig->GetString("map", "");
    m_Map.clear();
    for (const std::string& name : m_Maps)
    {
        if (!V_stricmp(name.c_str(), map))
            m_Map = name;
    }
}

void CImGuiCreateServer::DrawImGui()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(860, 580), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(640, 420), ImVec2(FLT_MAX, FLT_MAX));

    // without the border a checkbox is a blank square
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    if (m_bFocusWindow)
    {
        ImGui::SetNextWindowFocus();
        m_bFocusWindow = false;
    }

    bool open = true;
    std::string title = Localized("#GameUI_CreateServer") + "###CreateServer";
    if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse))
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        float footer = ImGui::GetFrameHeight() + style.ItemSpacing.y * 2.0f + style.WindowPadding.y;

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kRounding);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 12));
        ImGui::BeginChild("Maps", ImVec2(kMapListWidth, -footer), true);
        DrawMapList();
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPagePadding, kPagePadding));
        ImGui::BeginChild("Settings", ImVec2(0, -footer), true);
        DrawSettings();
        ImGui::EndChild();
        ImGui::PopStyleVar(2);

        DrawFooter();

        // in a level, Escape belongs to the game menu
        bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
        if (focused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape) && !GameUI().IsInLevel())
            open = false;
    }
    ImGui::End();

    ImGui::PopStyleVar();

    if (!open)
        Close();
}

void CImGuiCreateServer::DrawMapList()
{
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.0f);
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", Localized("#Cstrike_Listen_MapName", "Map").c_str());
    ImGui::Dummy(ImVec2(0, 2));

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##Search", Localized("#GameUI_CreateServerSearch", "Search maps").c_str(), m_szMapSearch, sizeof(m_szMapSearch));
    ImGui::Dummy(ImVec2(0, 4));

    ImGui::BeginChild("List", ImVec2(0, 0), false);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));

    std::string search = ToLower(m_szMapSearch);
    bool start = false;

    if (search.empty())
    {
        if (ListItem("##Random", Localized("#GameUI_CreateServerRandomMap", "Random map"), m_Map.empty()))
            m_Map.clear();
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            start = true;
    }

    int shown = 0;
    for (const std::string& name : m_Maps)
    {
        if (!search.empty() && ToLower(name).find(search) == std::string::npos)
            continue;

        shown++;
        bool selected = !V_stricmp(name.c_str(), m_Map.c_str());
        if (ListItem(name.c_str(), name, selected))
            m_Map = name;
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            start = true;

        if (selected && m_bScrollToMap)
            ImGui::SetScrollHereY(0.5f);
    }
    m_bScrollToMap = false;

    if (!search.empty() && shown == 0)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.0f);
        ImGui::TextDisabled("%s", Localized("#GameUI_CreateServerNoMaps", "No maps found").c_str());
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();

    if (start && !m_Maps.empty())
        Start();
}

void CImGuiCreateServer::DrawSettings()
{
    ImGui::PushFont(HeadingFont());
    ImGui::TextUnformatted(m_Map.empty() ? Localized("#GameUI_CreateServerRandomMap", "Random map").c_str() : m_Map.c_str());
    ImGui::PopFont();
    ImGui::TextDisabled("%s", Localized("#GameUI_CreateServerHint", "A server on this computer that friends on your network can join").c_str());
    ImGui::Dummy(ImVec2(0, 8));

    if (m_pRules)
    {
        BeginCard("#GameUI_Server", "Server");
        for (const char* cvar : kServerRules)
        {
            if (CScriptObject* option = m_pRules->FindObject(cvar))
                DrawRule(*option);
        }
        EndCard();
    }

    DrawBots();
    DrawRules();
}

void CImGuiCreateServer::DrawBots()
{
    BeginCard("#GameUI_CreateServerBots", "Bots");

    BeginRow("#Cstrike_Bot_IncludeBots", false);
    ImGui::Checkbox("##Enabled", &m_bBotsEnabled);
    EndRow();

    ImGui::BeginDisabled(!m_bBotsEnabled);

    BeginRow("#Cstrike_Bot_NumberOfBots", false);
    int quota = m_pBotConfig->GetInt("bot_quota");
    int maxPlayers = std::clamp(atoi(m_Rules["maxplayers"].c_str()), 2, 32);
    if (ImGui::SliderInt("##Quota", &quota, 0, maxPlayers - 1, "%d", ImGuiSliderFlags_AlwaysClamp))
        m_pBotConfig->SetInt("bot_quota", quota);
    EndRow();

    // the four levels as one row of buttons, the chosen one in the accent color
    BeginRow("#Cstrike_Bot_Difficulty", false);
    float width = ImGui::CalcItemWidth();
    float spacing = 4.0f;
    float buttonWidth = (width - spacing * 3.0f) / 4.0f;
    for (int i = 0; i < 4; i++)
    {
        if (i > 0)
            ImGui::SameLine(0.0f, spacing);

        bool selected = m_iBotDifficulty == i;
        if (selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(ImGuiCol_CheckMark, 0.85f));
        }
        ImGui::PushID(i);
        if (ImGui::Button(Localized(kBotDifficulties[i]).c_str(), ImVec2(buttonWidth, 0)))
            m_iBotDifficulty = i;
        ImGui::PopID();
        if (selected)
            ImGui::PopStyleColor(2);
    }
    EndRow();

    auto choiceCombo = [this](const char* token, const char* key, const BotChoice* choices, size_t count)
    {
        BeginRow(token, false);
        const char* current = m_pBotConfig->GetString(key);
        std::string preview = current;
        for (size_t i = 0; i < count; i++)
        {
            if (!V_stricmp(choices[i].value, current))
                preview = Localized(choices[i].token);
        }

        ImGui::PushID(key);
        if (ImGui::BeginCombo("##Value", preview.c_str()))
        {
            for (size_t i = 0; i < count; i++)
            {
                if (ImGui::Selectable(Localized(choices[i].token).c_str(), !V_stricmp(choices[i].value, current)))
                    m_pBotConfig->SetString(key, choices[i].value);
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
        EndRow();
    };

    choiceCombo("#CStrike_Bot_JoinTeam", "bot_join_team", kBotTeams, std::size(kBotTeams));
    choiceCombo("#Cstrike_Bot_Chatter", "bot_chatter", kBotChatter, std::size(kBotChatter));

    BeginRow("#CStrike_Bot_NamePrefix", false);
    char prefix[64];
    V_strncpy(prefix, m_pBotConfig->GetString("bot_prefix"), sizeof(prefix));
    if (ImGui::InputText("##Prefix", prefix, sizeof(prefix)))
    {
        UTIL_StripInvalidCharacters(prefix, sizeof(prefix));
        m_pBotConfig->SetString("bot_prefix", prefix);
    }
    EndRow();

    BotCheckbox("#CStrike_Bot_JoinAfterPlayer", "bot_join_after_player");
    BotCheckbox("#Cstrike_Bot_Defer", "bot_defer_to_human");
    BotCheckbox("#CStrike_Bot_GoRogue", "bot_allow_rogues");

    ImGui::EndDisabled();
    EndCard();

    // the weapons are short words, so they go two to a line instead of a row each
    BeginCard("#CStrike_Bot_AllowWeapon", "Bots can use");
    ImGui::BeginDisabled(!m_bBotsEnabled);
    float column = (m_flCardRight - ImGui::GetCursorScreenPos().x) * 0.5f;
    float left = ImGui::GetCursorPosX();
    for (size_t i = 0; i < std::size(kBotWeapons); i++)
    {
        if (i % 2)
            ImGui::SameLine(left + column);

        bool allowed = m_pBotConfig->GetInt(kBotWeapons[i].key) != 0;
        if (ImGui::Checkbox(Localized(kBotWeapons[i].token).c_str(), &allowed))
            m_pBotConfig->SetInt(kBotWeapons[i].key, allowed);
    }
    ImGui::EndDisabled();
    EndCard();
}

bool CImGuiCreateServer::BotCheckbox(const char* token, const char* key)
{
    BeginRow(token, false);
    bool value = m_pBotConfig->GetInt(key) != 0;
    ImGui::PushID(key);
    bool changed = ImGui::Checkbox("##Value", &value);
    ImGui::PopID();
    EndRow();

    if (changed)
        m_pBotConfig->SetInt(key, value);
    return changed;
}

void CImGuiCreateServer::DrawRules()
{
    if (!m_pRules)
        return;

    bool any = false;
    for (CScriptObject* option = m_pRules->pObjList; option; option = option->pNext)
    {
        if (option->type == O_OBSOLETE || IsServerRule(option->cvarname))
            continue;

        if (!any)
        {
            BeginCard("#GameUI_CreateServerRules", "Rules");
            any = true;
        }
        DrawRule(*option);
    }

    if (any)
        EndCard();
}

void CImGuiCreateServer::DrawRule(CScriptObject& option)
{
    ScriptOptionRow(option, m_Rules[option.cvarname], false);
}

void CImGuiCreateServer::DrawFooter()
{
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::Dummy(ImVec2(0, style.ItemSpacing.y));

    std::string start = Localized("#GameUI_Start");
    std::string cancel = Localized("#GameUI_Cancel");

    float buttonWidth = 96.0f;
    for (const std::string* text : { &start, &cancel })
        buttonWidth = std::max(buttonWidth, ImGui::CalcTextSize(text->c_str()).x + style.FramePadding.x * 2.0f);

    float rowWidth = buttonWidth * 2.0f + style.ItemSpacing.x;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - rowWidth));

    if (ImGui::Button(cancel.c_str(), ImVec2(buttonWidth, 0)))
        Close();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(ImGuiCol_CheckMark, 0.85f));
    ImGui::BeginDisabled(m_Maps.empty());
    if (ImGui::Button(start.c_str(), ImVec2(buttonWidth, 0)))
        Start();
    ImGui::EndDisabled();
    ImGui::PopStyleColor(2);
}

void CImGuiCreateServer::SaveRules()
{
    // the file keeps a list's index, where the page has the item's value
    for (CScriptObject* option = m_pRules->pObjList; option; option = option->pNext)
    {
        auto it = m_Rules.find(option->cvarname);
        if (it == m_Rules.end())
            continue;

        if (option->type == O_LIST)
        {
            int index = 0;
            for (CScriptListItem* item = option->pListItems; item; item = item->pNext, index++)
            {
                if (SameValue(item->szValue, it->second.c_str()))
                    option->SetCurValue(std::to_string(index).c_str());
            }
        }
        else
            option->SetCurValue(it->second.c_str());
    }

    FileHandle_t file = g_pFullFileSystem->Open("settings.scr", "wb");
    if (file != FILESYSTEM_INVALID_HANDLE)
    {
        m_pRules->WriteToScriptFile(file);
        g_pFullFileSystem->Close(file);
    }
}

void CImGuiCreateServer::Start()
{
    if (m_Maps.empty())
        return;

    std::string map = m_Map.empty() ? m_Maps[engine->pfnRandomLong(0, (int)m_Maps.size() - 1)] : m_Map;

    if (m_pRules)
        SaveRules();

    m_pBotConfig->SetInt("bot_difficulty", m_bBotsEnabled ? m_iBotDifficulty : -1);
    m_pBotConfig->SetString("map", m_Map.c_str());
    m_pBotConfig->SaveToFile(g_pFullFileSystem, "CSBotConfig.vdf");

    char command[1024];
    V_snprintf(command, sizeof(command), "disconnect\nsv_lan 1\nsetmaster enable\nmaxplayers %d\nsv_password \"%s\"\nhostname \"%s\"\ncd fadeout\nmap %s\n",
        std::clamp(atoi(m_Rules["maxplayers"].c_str()), 1, 32), m_Rules["sv_password"].c_str(), m_Rules["hostname"].c_str(), map.c_str());
    engine->pfnClientCmd(command);

    // wait a few frames after the map loads to make sure everything is "settled"
    engine->pfnClientCmd("wait\nwait\n");

    for (KeyValues* key = m_pBotConfig->GetFirstSubKey(); key; key = key->GetNextKey())
    {
        if (!V_stricmp(key->GetName(), "map"))
            continue;

        // the file remembers how many bots were chosen, but without bots there are none
        if (!V_stricmp(key->GetName(), "bot_quota") && !m_bBotsEnabled)
            V_snprintf(command, sizeof(command), "bot_quota 0\n");
        else
            V_snprintf(command, sizeof(command), "%s \"%s\"\n", key->GetName(), key->GetString());
        engine->pfnClientCmd(command);
    }

    engine->pfnClientCmd("mp_autoteambalance 0\n");
    engine->pfnClientCmd("mp_limitteams 0\n");

    m_bRulesPending = m_pRules != nullptr;
    GameUI().NeedApplyMultiplayerGameSettings();

    Close();
}

bool CImGuiCreateServer::ApplyServerSettings()
{
    if (!m_bRulesPending)
        return false;

    m_bRulesPending = false;
    m_pRules->WriteToConfig();
    return true;
}
