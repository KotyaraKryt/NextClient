#pragma once

#include "ImGuiForm.h"

#include <vgui_controls/PHandle.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// The options dialog drawn with Dear ImGui; opt_legacy 1 brings the VGUI one back.
// Like the old dialog, changes wait for OK or Apply and Cancel throws them away
class CBobPreviewPanel;
class CInfoDescription;
class CScriptObject;

class CImGuiOptions : public CImGuiFormPanel
{
    DECLARE_CLASS_SIMPLE(CImGuiOptions, CImGuiFormPanel);

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
    void PreparePreview() override;

    // while a key is being captured for a binding, the next key or button goes to it, not to ImGui
    void OnKeyCodePressed(vgui2::KeyCode code) override;
    void OnKeyCodeTyped(vgui2::KeyCode code) override;
    void OnMousePressed(vgui2::MouseCode code) override;
    void OnMouseDoublePressed(vgui2::MouseCode code) override;
    void OnMouseWheeled(int delta) override;

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
    void DrawVideo();
    void DrawMultiplayer();
    void DrawSpray();
    void DrawAdvancedOption(CScriptObject& option);
    void DrawGame();
    void DrawGamePreview(float height);
    void DrawCrosshairTab();
    void DrawBobbingTab();
    void DrawModelTab();
    void DrawInertiaTab();
    void DrawCameraTab();
    void DrawKeyboard();
    void DrawAppearance();
    // the chosen window, real and in a frame, over the menu's background
    void DrawAppearancePreview(ImGuiAppearance::Element element, const ImGuiAppearance::Values& values, const ImVec2& size);
    CImGuiPanel* PreviewPanel(ImGuiAppearance::Element element);
    // kb_act.lst's actions with the keys the engine has bound to them
    void LoadBindings();
    // kb_def.lst's keys in place of the current ones, waiting for Apply like any change
    void LoadDefaultBindings();
    void SaveBindings();
    // binds keyName to the action being captured, taking it off any other action first
    void FinishCapture(const char* keyName);
    // hands the preview the values the page shows, applied or not
    void SyncGamePreview();
    // sets the cvar's pending value back to what NextClient registers it with
    void ResetToDefault(const char* cvar);
    void SetPendingFloat(const char* cvar, float value);

    bool CvarCheckbox(const char* token, const char* cvar);
    // checked while the cvar is negative, like m_pitch for an inverted mouse
    bool CvarNegateCheckbox(const char* token, const char* cvar);
    // displayScale only changes what the slider shows, e.g. 100 for a 0-1 cvar shown in percent
    bool CvarSlider(const char* token, const char* cvar, float min, float max, const char* format, float displayScale = 1.0f, int flags = 0);
    bool CvarCombo(const char* token, const char* cvar, const std::vector<Choice>& choices);
    // a +command the player keeps on, like +mlook; keyName is the client's name for its state, like in_mlook
    bool KeyToggleCheckbox(const char* token, const char* keyName, const char* command);
    bool CvarText(const char* token, const char* cvar, bool password = false);

    // a cvar's value, or a userinfo key's for the names in m_SetInfoKeys
    std::string CurrentString(const char* name) const;
    bool Exists(const char* name) const;
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
    // the video mode and renderer come from the engine, the rest from two config files
    void LoadVideoSettings();
    // restartForCvars: brightness or gamma changed, which the engine only picks up on a restart
    void SaveVideoSettings(bool restartForCvars);
    // the sprays in logos/ and the options user.scr describes
    void LoadMultiplayerSettings();
    // user.scr keeps the last values chosen, for the old dialog to start from
    void SaveAdvancedOptions();
    // writes tempdecal.wad, the spray the engine uploads to servers
    void SaveSpray();
    void ApplyChanges();
    void Close();
    // the Game page, with its crosshair tab in front
    void ShowCrosshairSettings();

    std::vector<Page> m_Pages;
    const Page* m_pSelected = nullptr;
    bool m_bFocusWindow = false;

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

    struct VideoSettings
    {
        int width = 0;
        int height = 0;
        int bpp = 0;
        std::string renderer;
        int windowed = 0;
        int hdModels = 0;
        int addonsFolder = 0;
        int lowDetail = 0;
        int disableMultitexture = 0;
        int stretchAspect = 0;
        // VideoAdvancedSettings.vdf: the view model's FOV follows the main one
        bool viewmodelFovAuto = true;

        bool operator==(const VideoSettings&) const = default;
    };

    VideoSettings m_VideoSaved;
    VideoSettings m_VideoEdited;
    // width and height
    std::vector<std::pair<int, int>> m_VideoModes;

    // names in logos/ without .bmp
    std::vector<std::string> m_Logos;
    int m_iLogoTexture = 0;
    int m_iLogoWidth = 0;
    int m_iLogoHeight = 0;
    // the logo and color the texture shows, so it's only remade when they change
    std::string m_LogoTextureKey;
    std::unique_ptr<CInfoDescription> m_pAdvancedOptions;

    // the Game page's tabs, its preview of the view and how the preview moves
    int m_iGameTab = 0;
    bool m_bSelectGameTab = false;
    vgui2::DHANDLE<CBobPreviewPanel> m_hGamePreview;
    int m_iPreviewMove = 0;
    int m_iShownPreviewMove = -1;
    int m_iShownPreviewDemo = -1;
    bool m_bPreviewDrawn = false;

public:
    struct Binding
    {
        std::string command;
        // a token, or the text itself
        std::string description;
        // the row starts a section with description as its title
        bool header = false;
        std::string key;
        std::string altKey;
    };

private:

    // the list as the engine had it, and as the page shows it
    std::vector<Binding> m_BindingsSaved;
    std::vector<Binding> m_Bindings;
    // keys bound to the list's actions when it was loaded, which Apply unbinds before binding anew
    std::vector<std::string> m_KeysToUnbind;
    // the row and slot (0 the key, 1 the alternate) waiting for a key, -1 when none is
    int m_iCaptureRow = -1;
    int m_iCaptureSlot = 0;
    char m_szBindingSearch[64] = {};
    bool m_bOpenDefaultsPopup = false;

    // the window the Interface page shows the settings of, an ImGuiAppearance::Element
    int m_iAppearanceElement = 0;
    // the windows the Interface page draws its previews with, made the first time each is chosen
    vgui2::DHANDLE<CImGuiPanel> m_PreviewPanels[static_cast<int>(ImGuiAppearance::Element::Count)];

    // userinfo keys that Apply sets with setinfo instead of as cvars
    std::set<std::string> m_SetInfoKeys;
};
