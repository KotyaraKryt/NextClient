// DemoPlayerFileDialog.cpp: implementation of the CDemoPlayerFileDialog class.
//
//////////////////////////////////////////////////////////////////////

#include "DemoPlayerFileDialog.h"

#include <string>

using namespace vgui2;

#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Button.h>
#include <KeyValues.h>
#include <vgui_controls/ListPanel.h>

#include <cvars/cvar_defaults.h>

#include "FileSystem.h"
#include "GameUi.h"
// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDemoPlayerFileDialog::CDemoPlayerFileDialog(vgui2::Panel *parent, const char *name): Frame ( parent, name )
{
    SetBounds(0, 0, 278, 380);
    SetSizeable( false );

    surface()->CreatePopup( GetVPanel(), false );

    SetTitle( "#GameUI_LoadDemo", true );

    m_pDemoList = new ListPanel(this, "DemoList");

    Button * load = new Button(this, "LoadButton", "#GameUI_Load");
    Button * cancel = new Button(this, "CancelButton", "#GameUI_Cancel");

    LoadControlSettings("Resource/DemoPlayerFileDialog.res");

    m_pDemoList->AddColumnHeader(0, "demoname", "#GameUI_DemoFile", m_pDemoList->GetWide());

    LoadDemoList();
}

CDemoPlayerFileDialog::~CDemoPlayerFileDialog()
{

}

void CDemoPlayerFileDialog::LoadDemoList()
{
    // clear the current list (if any)
    m_pDemoList->DeleteAllItems();

    AddDemosFromFolder("");

    const char* demo_folder = engine->pfnGetCvarString(cvars::kDemoFolder.name);
    if (demo_folder[0] != '\0')
    {
        AddDemosFromFolder(demo_folder);
    }

    // set the first item to be selected
    if (m_pDemoList->GetItemCount() > 0)
    {
        int itemID = m_pDemoList->GetItemIDFromRow(0);
        m_pDemoList->SetSingleSelectedItem(itemID);
    }
}

void CDemoPlayerFileDialog::AddDemosFromFolder(const char* folder)
{
    std::string prefix = folder[0] != '\0' ? std::string(folder) + "/" : std::string();

    FileFindHandle_t find_handle = NULL;
    const char* filename = g_pFullFileSystem->FindFirst((prefix + "*.dem").c_str(), &find_handle, "GAME");
    while (filename)
    {
        m_pDemoList->AddItem(new KeyValues("data", "demoname", (prefix + filename).c_str()), 0, false, false);
        filename = g_pFullFileSystem->FindNext(find_handle);
    }

    g_pFullFileSystem->FindClose(find_handle);
}

void CDemoPlayerFileDialog::OnCommand( const char *command )
{
    if ( !strcmp( command, "load" ) )
    {
        // get first selected item
        int itemID = m_pDemoList->GetSelectedItem(0);
        if ( m_pDemoList->IsValidItemID(itemID) )
        {
            KeyValues *kv = m_pDemoList->GetItem(itemID);
            PostActionSignal(new KeyValues("DemoSelected", "demoname", kv->GetString("demoname", "") ));
        }

        OnClose();
    }

    BaseClass::OnCommand(command);
}

void CDemoPlayerFileDialog::OnClose()
{
    BaseClass::OnClose();
    MarkForDeletion();
}