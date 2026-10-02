#pragma once

#include "ImGuiPanel.h"

#include <map>
#include <string>
#include <vector>

// The options dialog drawn with Dear ImGui; opt_legacy 1 brings the VGUI one back.
// Like the old dialog, changes wait for OK or Apply and Cancel throws them away
class CImGuiOptions : public CImGuiPanel
{
    DECLARE_CLASS_SIMPLE(CImGuiOptions, CImGuiPanel);

public:
    CImGuiOptions();

    // the cvar has to exist before config.cfg runs, or its saved value is lost
    static void RegisterCvars();
    static bool UseLegacyDialog();

    // tabName is one of the old dialog's: "multiplayer", "audio", ...; nullptr keeps the current page
    void Activate(const char* tabName = nullptr);

protected:
    void DrawImGui() override;

private:
    struct Page
    {
        const char* id;
        const char* token;
        // the pages are listed in groups, which follow each other in m_Pages
        int group;
        // nullptr for a page that only the old dialog has so far
        void (CImGuiOptions::*draw)();
        // the line under the page's heading, as a token and the English for languages without it
        const char* hintToken;
        const char* hintEnglish;
    };

    // one of a combo box's choices: what it shows and the value it gives the cvar
    struct Choice
    {
        const char* token;
        const char* value;
    };

    void DrawPageList();
    void DrawPageHeading(const Page& page);
    void DrawFooter();
    void DrawNotPorted(const Page& page);
    void DrawAudio();

    // a titled box the settings rows go in; it has to be closed before the page's next one
    void BeginCard(const char* token, const char* english);
    void EndCard();
    // a row's caption on the left, then the control comes in the right part of the card
    void SettingRow(const char* token, const char* cvar);
    bool CvarCheckbox(const char* token, const char* cvar);
    // displayScale only changes what the slider shows, e.g. 100 for a 0-1 cvar shown in percent
    bool CvarSlider(const char* token, const char* cvar, float min, float max, const char* format, float displayScale = 1.0f);
    bool CvarCombo(const char* token, const char* cvar, const std::vector<Choice>& choices);

    // what the cvar will be after Apply: the pending value if there is one
    std::string PendingString(const char* cvar) const;
    float PendingValue(const char* cvar) const;
    void SetPending(const char* cvar, const std::string& value);
    void ApplyChanges();
    void Close();

    std::vector<Page> m_Pages;
    const Page* m_pSelected = nullptr;
    bool m_bFocusWindow = false;

    // the open card's top left corner and the right edge its controls end at, in screen space
    float m_flCardLeft = 0.0f;
    float m_flCardTop = 0.0f;
    float m_flCardRight = 0.0f;

    // cvar name -> the value it gets on Apply
    std::map<std::string, std::string> m_Pending;
};
