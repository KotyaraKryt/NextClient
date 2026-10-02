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
    ~CImGuiOptions() override;

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
    void DrawMouse();
    void DrawMisc();
    void DrawVoice();

    // a titled box the settings rows go in; it has to be closed before the page's next one
    void BeginCard(const char* token, const char* english);
    void EndCard();
    // a line of smaller text under the next row's caption, like ImGui's SetNext* functions
    void SetNextRowHint(const char* token);
    // a row's caption on the left, then the control comes in the right part of the card;
    // pending puts a dot by a row that Apply would change
    void BeginRow(const char* token, bool pending);
    void EndRow();

    bool CvarCheckbox(const char* token, const char* cvar);
    // checked while the cvar is negative, like m_pitch for an inverted mouse
    bool CvarNegateCheckbox(const char* token, const char* cvar);
    // displayScale only changes what the slider shows, e.g. 100 for a 0-1 cvar shown in percent
    bool CvarSlider(const char* token, const char* cvar, float min, float max, const char* format, float displayScale = 1.0f, int flags = 0);
    bool CvarCombo(const char* token, const char* cvar, const std::vector<Choice>& choices);
    // a +command the player keeps on, like +mlook; keyName is the client's name for its state, like in_mlook
    bool KeyToggleCheckbox(const char* token, const char* keyName, const char* command);

    // what the cvar will be after Apply: the pending value if there is one
    std::string PendingString(const char* cvar) const;
    float PendingValue(const char* cvar) const;
    void SetPending(const char* cvar, const std::string& value);
    size_t PendingCount() const;
    // MiscellaneousSettings.vdf, which the Misc page edits instead of cvars
    void LoadMiscSettings();
    void SaveMiscSettings();
    // the microphone's volume and boost live in the engine's voice tweak interface, not in cvars
    void LoadVoiceSettings();
    void SaveVoiceSettings();
    void StartMicrophoneTest();
    void StopMicrophoneTest();
    void ApplyChanges();
    void Close();

    std::vector<Page> m_Pages;
    const Page* m_pSelected = nullptr;
    bool m_bFocusWindow = false;

    // the open card's top left corner and the right edge its controls end at, in screen space
    float m_flCardLeft = 0.0f;
    float m_flCardTop = 0.0f;
    float m_flCardRight = 0.0f;

    // where the open row's caption went, and the hint to put under it
    float m_flRowLeft = 0.0f;
    float m_flRowTop = 0.0f;
    float m_flRowCaptionRight = 0.0f;
    const char* m_pszRowHint = nullptr;

    // cvar name -> the value it gets on Apply
    std::map<std::string, std::string> m_Pending;
    // +command name -> whether Apply turns it on
    std::map<std::string, bool> m_PendingKeys;

    struct MiscSettings
    {
        std::string scheme;
        int serverBrowserTab = 0;
        bool disableAutoOpenServerBrowser = false;

        bool operator==(const MiscSettings&) const = default;
    };

    // what the file has and what the page shows; Apply writes the second when they differ
    MiscSettings m_MiscSaved;
    MiscSettings m_MiscEdited;
    // scheme file names in resource/schemes
    std::vector<std::string> m_Schemes;

    struct VoiceSettings
    {
        int microphoneVolume = 0;
        bool microphoneBoost = false;

        bool operator==(const VoiceSettings&) const = default;
    };

    VoiceSettings m_VoiceSaved;
    VoiceSettings m_VoiceEdited;
    bool m_bTestingMicrophone = false;
    // voice_scale as it was before a test played the pending value
    std::string m_VoiceScaleBeforeTest;
};
