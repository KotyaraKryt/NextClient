//========= Copyright ?1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "GameUi.h"
#include "GameConsole.h"
#include "GameConsoleNext.h"
#include "GameConsoleDialog.h"
#include "LoadingDialog.h"
#include "ImGuiConsole.h"
#include "ImGuiPanel.h"
#include <console_buffer/log_file.h>
#include <cvardef.h>
#include <tier1/strtools.h>
#include <vgui/ILocalize.h>
#include <FileSystem.h>
#include <algorithm>
#include <imgui/imgui.h>
#include <vgui/ISurfaceNext.h>

#include <KeyValues.h>
#include <vgui/Cursor.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static CGameConsole g_GameConsole;
//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CGameConsole &GameConsole()
{
    return g_GameConsole;
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameConsole, IGameConsole, GAMECONSOLE_INTERFACE_VERSION_GS, g_GameConsole);

class CImGuiDemoPanel : public CImGuiPanel
{
protected:
    void DrawImGui() override
    {
        bool open = true;
        ImGui::ShowDemoWindow(&open);
        if (!open)
            SetVisible(false);
    }
};

static CImGuiDemoPanel* g_pImGuiDemo = nullptr;

static void OnCmdImGuiDemo()
{
    if (!g_pImGuiDemo)
        g_pImGuiDemo = vgui2::SETUP_PANEL(new CImGuiDemoPanel());

    bool show = !g_pImGuiDemo->IsVisible();
    g_pImGuiDemo->SetVisible(show);
    if (show)
    {
        g_pImGuiDemo->MoveToFront();
        g_pImGuiDemo->RequestFocus();
    }
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameConsole::CGameConsole()
{
    m_bInitialized = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGameConsole::~CGameConsole()
{
    m_bInitialized = false;
}

//-----------------------------------------------------------------------------
// Purpose: sets up the console for use
//-----------------------------------------------------------------------------
void CGameConsole::Initialize()
{
    if (m_bInitialized)
        return;

    m_pConsole = vgui2::SETUP_PANEL( new CGameConsoleDialog() ); // we add text before displaying this so set it up now!
    int swide, stall;
    //m_pConsole->SetParent(g_pTaskbar->GetVPanel());

    vgui2::surface()->GetScreenSize(swide, stall);
    int offset = 40;
    m_pConsole->SetBounds(
        offset, offset,
        std::min( swide - 2 * offset, 560 ), std::min( stall - 2 * offset, 400 ) );

    GameConsoleNext().Initialize(m_pConsole);

    // the ImGui console's settings, kept in config.cfg; its gear button sets them too
    static const char* const kSettings[][2] = {
        { "con_timestamps", "0" },
        { "con_log", "1" },
        { "con_restore", "1" },
        { "con_collapse", "1" },
        { "con_wrap", "1" },
        { "con_fontsize", "16" },
        { "con_opacity", "1" },
        { "con_maxlines", "5000" },
        { "con_keepfilters", "1" },
        { "con_filters", "" },
        { "con_suggestions", "1" },
    };
    for (const auto& setting : kSettings)
        engine->pfnRegisterVariable(setting[0], setting[1], FCVAR_ARCHIVE);

    m_pLegacyCvar = engine->pfnRegisterVariable("con_legacy", "0", FCVAR_ARCHIVE);
    m_pImGuiConsole = vgui2::SETUP_PANEL(new CImGuiConsole(m_Scrollback));

    OpenLog();
    m_Scrollback.SetLineClosedHandler([this](const console_buffer::Line& line) { WriteToLog(line); });

    m_bInitialized = true;

    engine->pfnAddCommand("condump", CGameConsole::OnCmdCondump);
    engine->pfnAddCommand("clear_conlogs", CGameConsole::OnCmdClearConLogs);
    engine->pfnAddCommand("imgui_demo", OnCmdImGuiDemo);

    // This provides a 1 frame delay to display the text after the temporary buffer from the engine
    TaskCoro::RunInMainThread([this]
    {
        ExecuteTempConsoleBuffer();
    });
}

//-----------------------------------------------------------------------------
// Purpose: activates the console, makes it visible and brings it to the foreground
//-----------------------------------------------------------------------------
void CGameConsole::Activate()
{
    if (!m_bInitialized)
        return;

    if ( LoadingDialog() )
        return;

    vgui2::surface()->RestrictPaintToSinglePanel(NULL);

    if (UseLegacyConsole())
        m_pConsole->Activate();
    else
        m_pImGuiConsole->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: hides the console
//-----------------------------------------------------------------------------
void CGameConsole::Hide()
{
    if (!m_bInitialized)
        return;

    if (GameUI().IsInLevel())
        m_pConsole->SetFadeEffectDisableOverride(true);

    m_pConsole->Hide();
    m_pImGuiConsole->SetVisible(false);

    if (GameUI().IsInLevel())
        m_pConsole->SetFadeEffectDisableOverride(false);
}

//-----------------------------------------------------------------------------
// Purpose: clears the console
//-----------------------------------------------------------------------------
void CGameConsole::Clear()
{
    if (!m_bInitialized)
        return;

    m_pConsole->Clear();
    m_Scrollback.Clear();
}

//-----------------------------------------------------------------------------
// Purpose: prints a message to the console
//-----------------------------------------------------------------------------
void CGameConsole::Printf(const char *format, ...)
{
    va_list argptr;
    char msg[4096];

        va_start(argptr, format);
    Q_vsnprintf(msg, sizeof(msg), format, argptr);
    msg[sizeof(msg) - 1] = 0;
        va_end(argptr);

    if (!m_bInitialized)
    {
        m_TempConsoleBuffer.emplace_back(msg, false);
    }
    else
    {
        m_pConsole->Print(msg);
    }
}

void CGameConsole::PrintfWithoutJsEvent(Color color, const std::wstring& msg)
{
    if (!m_bInitialized)
    {
        // I'm lazy
    }
    else
    {
        m_pConsole->ColorPrintWithoutJsEvent(color, msg.c_str());
    }
}

void CGameConsole::PrintfWithoutJsEvent(Color color, const std::string& msg)
{
    if (!m_bInitialized)
    {
        m_TempConsoleBuffer.emplace_back(msg, false);
    }
    else
    {
        m_pConsole->ColorPrintWithoutJsEvent(color, msg.c_str());
    }
}

void CGameConsole::ExecuteTempConsoleBuffer()
{
    for (const SavedMessageData& msg : m_TempConsoleBuffer)
    {
        if (msg.is_developer)
        {
            DPrintf("%s", msg.text.c_str());
        }
        else
        {
            Printf("%s", msg.text.c_str());
        }
    }

    m_TempConsoleBuffer.clear();
    m_TempConsoleBuffer.shrink_to_fit();
}

//-----------------------------------------------------------------------------
// Purpose: printes a debug message to the console
//-----------------------------------------------------------------------------
void CGameConsole::DPrintf(const char *format, ...)
{
    va_list argptr;
    char msg[4096];

        va_start(argptr, format);
    Q_vsnprintf(msg, sizeof(msg), format, argptr);
    msg[sizeof(msg) - 1] = 0;
        va_end(argptr);


    if (!m_bInitialized)
    {
        m_TempConsoleBuffer.emplace_back(msg, true);
    }
    else
    {
        m_pConsole->DPrint(msg);
    }
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the console is currently in focus
//-----------------------------------------------------------------------------
bool CGameConsole::IsConsoleVisible()
{
    if (!m_bInitialized)
        return false;

    return m_pConsole->IsVisible() || m_pImGuiConsole->IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose: activates the console after a delay
//-----------------------------------------------------------------------------
void CGameConsole::ActivateDelayed(float time)
{
    if (!m_bInitialized)
        return;

    m_pConsole->PostMessage(m_pConsole, new KeyValues("Activate"), time);
}

void CGameConsole::SetParent(int parent)
{
    if (!m_bInitialized)
        return;

    m_pConsole->SetParent( static_cast<vgui2::VPANEL>( parent ));
    m_pImGuiConsole->SetParent( static_cast<vgui2::VPANEL>( parent ));
}

static const char* const kLogFile = "console.log";
// the most con_maxlines can ask for
static const size_t kMaxLogLines = 20000;

void CGameConsole::OpenLog()
{
    // keep the newest lines of the earlier runs and write only those back, so the file
    // doesn't grow forever; this run's lines go after them
    FileHandle_t file = g_pFullFileSystem->Open(kLogFile, "rb");
    if (file != FILESYSTEM_INVALID_HANDLE)
    {
        std::string text(g_pFullFileSystem->Size(file), '\0');
        g_pFullFileSystem->Read(text.data(), static_cast<int>(text.size()), file);
        g_pFullFileSystem->Close(file);

        size_t start = 0;
        while (start < text.size())
        {
            size_t newline = text.find('\n', start);
            if (newline == std::string::npos)
                newline = text.size();

            m_PreviousLog.emplace_back(text, start, newline - start);
            start = newline + 1;
        }

        if (m_PreviousLog.size() > kMaxLogLines)
            m_PreviousLog.erase(m_PreviousLog.begin(), m_PreviousLog.end() - kMaxLogLines);
    }

    m_hLog = g_pFullFileSystem->Open(kLogFile, "wb");
    if (m_hLog == FILESYSTEM_INVALID_HANDLE)
    {
        m_hLog = nullptr;
        return;
    }

    for (const std::string& line : m_PreviousLog)
    {
        g_pFullFileSystem->Write(line.data(), static_cast<int>(line.size()), m_hLog);
        g_pFullFileSystem->Write("\n", 1, m_hLog);
    }
    g_pFullFileSystem->Flush(m_hLog);
}

void CGameConsole::WriteToLog(const console_buffer::Line& line)
{
    cvar_t* log = engine->pfnGetCvarPointer("con_log");
    if (!m_hLog || (log && log->value == 0.0f))
        return;

    std::string text = console_buffer::FormatLogLine(line) + "\n";
    g_pFullFileSystem->Write(text.data(), static_cast<int>(text.size()), m_hLog);
    // a crash would lose whatever hasn't been flushed, and that's when the log matters most
    g_pFullFileSystem->Flush(m_hLog);
}

void CGameConsole::OnCmdClearConLogs()
{
    g_GameConsole.ClearLogs();
}

// Unlike clear, which empties the screen, this forgets what the earlier runs left behind:
// console.log starts over and their lines leave the console, this run's stay
void CGameConsole::ClearLogs()
{
    if (m_hLog)
        g_pFullFileSystem->Close(m_hLog);

    m_hLog = g_pFullFileSystem->Open(kLogFile, "wb");
    if (m_hLog == FILESYSTEM_INVALID_HANDLE)
        m_hLog = nullptr;

    m_PreviousLog.clear();
    m_PreviousLog.shrink_to_fit();
    m_bRestored = true;
    m_Scrollback.RemoveEarlierLines();

    const wchar_t* message = g_pVGuiLocalize->Find("#Console_LogsCleared");
    char text[256] = "console.log cleared";
    if (message)
        V_UnicodeToUTF8(message, text, sizeof(text));
    Printf("%s\n", text);
}

void CGameConsole::RestorePreviousSession()
{
    if (m_bRestored)
        return;
    m_bRestored = true;

    cvar_t* restore = engine->pfnGetCvarPointer("con_restore");
    cvar_t* maxLines = engine->pfnGetCvarPointer("con_maxlines");
    if (restore && restore->value != 0.0f)
    {
        size_t keep = maxLines ? static_cast<size_t>(std::clamp(maxLines->value, 100.0f, 20000.0f)) : 5000;
        size_t first = m_PreviousLog.size() > keep ? m_PreviousLog.size() - keep : 0;

        std::vector<console_buffer::Line> lines;
        for (size_t i = first; i < m_PreviousLog.size(); i++)
        {
            if (std::optional<console_buffer::Line> line = console_buffer::ParseLogLine(m_PreviousLog[i]))
                lines.push_back(std::move(*line));
        }

        if (!lines.empty())
        {
            console_buffer::Line separator;
            const wchar_t* label = g_pVGuiLocalize->Find("#Console_PreviousRun");
            char text[256] = "previous run";
            if (label)
                V_UnicodeToUTF8(label, text, sizeof(text));

            separator.text = text;
            separator.segments.push_back({ console_buffer::Rgba{}, separator.text, true });
            separator.time = lines.back().time;
            separator.previous_session = true;
            separator.divider = true;
            lines.push_back(std::move(separator));

            m_Scrollback.AddEarlierLines(std::move(lines));
        }
    }

    m_PreviousLog.clear();
    m_PreviousLog.shrink_to_fit();
}

bool CGameConsole::UseLegacyConsole() const
{
    return m_pLegacyCvar && m_pLegacyCvar->value != 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: static command handler
//-----------------------------------------------------------------------------
void CGameConsole::OnCmdCondump()
{
    g_GameConsole.m_pConsole->DumpConsoleTextToFile();
}
