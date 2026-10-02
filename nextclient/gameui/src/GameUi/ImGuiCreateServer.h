#pragma once

#include "ImGuiForm.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class CServerDescription;
class KeyValues;

// The New Game dialog drawn with Dear ImGui; newgame_legacy 1 brings the VGUI one back.
// It keeps the old dialog's files: the rules in settings.scr, the bots and the last map in CSBotConfig.vdf
class CImGuiCreateServer : public CImGuiFormPanel
{
    DECLARE_CLASS_SIMPLE(CImGuiCreateServer, CImGuiFormPanel);

public:
    CImGuiCreateServer();
    ~CImGuiCreateServer() override;

    static void RegisterCvars();
    static bool UseLegacyDialog();

    void Activate();
    void Close();

    // sets the rules once the started server is up, like the old dialog; false if this dialog didn't start it
    bool ApplyServerSettings();

protected:
    void DrawImGui() override;

private:
    void LoadMaps();
    void LoadMapsFrom(const char* pathID);
    void LoadRules();
    void LoadBotConfig();

    void DrawMapList();
    void DrawSettings();
    void DrawBots();
    void DrawRules();
    void DrawFooter();
    void DrawRule(CScriptObject& option);

    bool BotCheckbox(const char* token, const char* key);
    void Start();
    void SaveRules();

    std::vector<std::string> m_Maps;
    // empty for a random map
    std::string m_Map;
    char m_szMapSearch[64] = {};
    bool m_bScrollToMap = false;
    bool m_bFocusWindow = false;

    std::unique_ptr<CServerDescription> m_pRules;
    // cvar -> the value the page shows; a list's is the item's value, not its index like in the file
    std::map<std::string, std::string> m_Rules;
    // the rules wait for the server to be up, then go to the console from m_pRules
    bool m_bRulesPending = false;

    KeyValues* m_pBotConfig = nullptr;
    bool m_bBotsEnabled = false;
    int m_iBotDifficulty = 0;
};
