#ifndef CREATEMULTIPLAYERGAMEGAMEPLAYPAGE_H
#define CREATEMULTIPLAYERGAMEGAMEPLAYPAGE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>
#include "ScriptObject.h"

class CPanelListPanel;
class mpcontrol_t;

//-----------------------------------------------------------------------------
// Purpose: class for loading/saving server config file
//-----------------------------------------------------------------------------
class CServerDescription : public CDescription
{
public:
    CServerDescription( CPanelListPanel *panel );

    void WriteScriptHeader( FileHandle_t fp );
    void WriteFileHeader( FileHandle_t fp );
};

//-----------------------------------------------------------------------------
// Purpose: server options page of the create game server dialog
//-----------------------------------------------------------------------------
class CCreateMultiplayerGameGameplayPage : public vgui2::PropertyPage
{
public:
    CCreateMultiplayerGameGameplayPage(vgui2::Panel *parent, const char *name);
    ~CCreateMultiplayerGameGameplayPage() override;

    // returns currently entered information about the server
    int GetMaxPlayers();
    const char *GetPassword();
    const char *GetHostName();

    void ApplySettings();

protected:
    void OnApplyChanges() override;

private:
    const char *GetValue(const char *cvarName, const char *defaultValue);
    void LoadGameOptionsList();
    void GatherCurrentValues();

    CDescription *m_pDescription;
    mpcontrol_t *m_pList;
    CPanelListPanel *m_pOptionsList;
};


#endif // CREATEMULTIPLAYERGAMEGAMEPLAYPAGE_H
