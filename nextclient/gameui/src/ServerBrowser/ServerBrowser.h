#ifndef SERVERBROWSER_H
#define SERVERBROWSER_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI2.h>
#include "../IServerBrowserEx.h"

#include <vgui_controls/PHandle.h>

class CServerBrowserDialog;
class CImGuiServerBrowser;
struct cvar_s;

class CServerBrowser : public IServerBrowserEx
{
public:
    CServerBrowser();
    ~CServerBrowser() override;

public:
    bool Initialize(CreateInterfaceFn *factorylist, int numFactories) override;
    bool Activate() override;
    bool Activate(ServerBrowserTab tab) override;
    void Shutdown() override;
    void SetParent(vgui2::VPANEL parent) override;
    void Deactivate() override;
    void Reactivate() override;
    void ActiveGameName(const char *szGameName, const char *szGameDir) override;
    void ConnectToGame(int ip, int connectionport) override;
    void DisconnectFromGame() override;
    bool JoinGame(unsigned int gameIP, unsigned int gamePort, const char *userName) override;
    void CloseAllGameInfoDialogs() override;

    virtual vgui2::VPANEL GetPanel();
    virtual void CreateDialog();

private:
    bool UseLegacyBrowser();

    vgui2::DHANDLE<CServerBrowserDialog> server_browser_dialog_{};
    vgui2::DHANDLE<CImGuiServerBrowser> imgui_browser_{};
    cvar_s* legacy_cvar_{};
    bool first_activate_passed_{};
};

CServerBrowser &ServerBrowser();

#endif