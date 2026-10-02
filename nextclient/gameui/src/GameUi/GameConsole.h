//========= Copyright ?1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef GAMECONSOLE_H
#define GAMECONSOLE_H
#ifdef _WIN32
#pragma once
#endif

#include <IGameConsole.h>
#include <console_buffer/console_buffer.h>

#include <string>
#include <vector>

class CGameConsoleDialog;
class CImGuiConsole;
struct cvar_s;

//-----------------------------------------------------------------------------
// Purpose: VGui implementation of the game/dev console
//-----------------------------------------------------------------------------
class CGameConsole : public IGameConsole
{
    struct SavedMessageData
    {
        std::string text;
        bool is_developer;

        explicit SavedMessageData(const std::string& text, bool is_developer) :
            text(text),
            is_developer(is_developer)
        { }
    };

public:
    CGameConsole();
    ~CGameConsole();

    // sets up the console for use
    void Initialize();

    // activates the console, makes it visible and brings it to the foreground
    virtual void Activate();
    // hides the console
    virtual void Hide();
    // clears the console
    virtual void Clear();
    // prints a message to the console
    virtual void Printf(const char *format, ...);
    // prints a debug message to the console
    virtual void DPrintf(const char *format, ...);
    // returns true if the console is currently in focus
    virtual bool IsConsoleVisible();

    // activates the console after a delay
    void ActivateDelayed(float time);

    void SetParent(int parent);

    static void OnCmdCondump();

    void PrintfWithoutJsEvent(Color color, const std::string& msg);
    void PrintfWithoutJsEvent(Color color, const std::wstring& msg);

    // everything the console shows, for the ImGui console to draw
    console_buffer::ConsoleBuffer& Scrollback() { return m_Scrollback; }

    // Puts the lines console.log kept from the last runs in front of this run's, once, if
    // con_restore asks for it. The ImGui console calls it when first opened: when the console
    // is set up, config.cfg hasn't been run yet and con_restore still has its default.
    void RestorePreviousSession();

private:
    void ExecuteTempConsoleBuffer();
    bool UseLegacyConsole() const;
    void OpenLog();
    void WriteToLog(const console_buffer::Line& line);

private:
    bool m_bInitialized;
    CGameConsoleDialog *m_pConsole;
    CImGuiConsole *m_pImGuiConsole = nullptr;
    cvar_s *m_pLegacyCvar = nullptr;
    console_buffer::ConsoleBuffer m_Scrollback;
    std::vector<SavedMessageData> m_TempConsoleBuffer;

    // console.log, open for this run's lines, and the earlier runs' lines it held
    void* m_hLog = nullptr;
    std::vector<std::string> m_PreviousLog;
    bool m_bRestored = false;
};

extern CGameConsole &GameConsole();

#endif // GAMECONSOLE_H
