#include "ImGuiPanel.h"
#include "ImGuiTheme.h"

#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Controls.h>
#include <FileSystem.h>
#include <tier1/strtools.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl2.h>
#include <imgui/imgui_internal.h>

#ifdef _WIN32
#include <windows.h>
// a macro for GetTickCount there, which would take ISystem::GetCurrentTime's place
#undef GetCurrentTime
#endif
#include <GL/gl.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui2;

static ImGuiKey ToImGuiKey(KeyCode code)
{
    if (code >= KEY_A && code <= KEY_Z)
        return static_cast<ImGuiKey>(ImGuiKey_A + (code - KEY_A));
    if (code >= KEY_0 && code <= KEY_9)
        return static_cast<ImGuiKey>(ImGuiKey_0 + (code - KEY_0));
    if (code >= KEY_F1 && code <= KEY_F12)
        return static_cast<ImGuiKey>(ImGuiKey_F1 + (code - KEY_F1));

    switch (code)
    {
        case KEY_TAB:       return ImGuiKey_Tab;
        case KEY_LEFT:      return ImGuiKey_LeftArrow;
        case KEY_RIGHT:     return ImGuiKey_RightArrow;
        case KEY_UP:        return ImGuiKey_UpArrow;
        case KEY_DOWN:      return ImGuiKey_DownArrow;
        case KEY_PAGEUP:    return ImGuiKey_PageUp;
        case KEY_PAGEDOWN:  return ImGuiKey_PageDown;
        case KEY_HOME:      return ImGuiKey_Home;
        case KEY_END:       return ImGuiKey_End;
        case KEY_INSERT:    return ImGuiKey_Insert;
        case KEY_DELETE:    return ImGuiKey_Delete;
        case KEY_BACKSPACE: return ImGuiKey_Backspace;
        case KEY_SPACE:     return ImGuiKey_Space;
        case KEY_ENTER:     return ImGuiKey_Enter;
        case KEY_PAD_ENTER: return ImGuiKey_KeypadEnter;
        case KEY_ESCAPE:    return ImGuiKey_Escape;
        case KEY_LCONTROL:  return ImGuiKey_LeftCtrl;
        case KEY_RCONTROL:  return ImGuiKey_RightCtrl;
        case KEY_LSHIFT:    return ImGuiKey_LeftShift;
        case KEY_RSHIFT:    return ImGuiKey_RightShift;
        case KEY_LALT:      return ImGuiKey_LeftAlt;
        case KEY_RALT:      return ImGuiKey_RightAlt;
        default:            return ImGuiKey_None;
    }
}

// the file's bytes in memory from IM_ALLOC, which an ImGui font atlas frees itself; nullptr if missing
static void* ReadWholeFile(const char* path, int& size)
{
    FileHandle_t file = g_pFullFileSystem->Open(path, "rb");
    if (file == FILESYSTEM_INVALID_HANDLE)
        return nullptr;

    size = g_pFullFileSystem->Size(file);
    if (size <= 0)
    {
        g_pFullFileSystem->Close(file);
        return nullptr;
    }

    void* data = IM_ALLOC(size);
    g_pFullFileSystem->Read(data, size, file);
    g_pFullFileSystem->Close(file);
    return data;
}

static void LoadFonts(ImGuiIO& io, float size, float titleScale)
{
    // Latin, Greek and Cyrillic, plus the punctuation, arrows, box drawing and shapes that servers
    // and plugins like to decorate their messages with
    static const ImWchar textRanges[] = {
        0x0020, 0x00FF, // Basic Latin, Latin-1
        0x0100, 0x017F, // Latin Extended-A
        0x0180, 0x024F, // Latin Extended-B
        0x0370, 0x03FF, // Greek
        0x0400, 0x052F, // Cyrillic
        0x2000, 0x206F, // General Punctuation
        0x2190, 0x21FF, // Arrows
        0x2500, 0x25FF, // Box Drawing, Block Elements, Geometric Shapes
        0,
    };

    // DejaVu Sans fills in what JetBrains Mono lacks: the rest of Latin Extended-B and Greek,
    // the scripts server names come in, and ★ ⚙ ✔ with the other symbols. Arabic is left out,
    // since ImGui can neither join its letters nor write it right to left
    static const ImWchar symbolRanges[] = {
        0x0180, 0x024F, // Latin Extended-B
        0x0370, 0x03FF, // Greek
        0x0530, 0x058F, // Armenian
        0x0590, 0x05FF, // Hebrew
        0x10A0, 0x10FF, // Georgian
        0x2300, 0x23FF, // Miscellaneous Technical
        0x25A0, 0x25FF, // Geometric Shapes, which JetBrains Mono only has a few of
        0x2600, 0x26FF, // Miscellaneous Symbols
        0x2700, 0x27BF, // Dingbats
        0,
    };

    auto addFont = [&](float fontSize)
    {
        int fileSize;
        void* data = ReadWholeFile("resource/fonts/JetBrainsMono-Regular.ttf", fileSize);
        if (!data)
            return;

        io.Fonts->AddFontFromMemoryTTF(data, fileSize, fontSize, nullptr, textRanges);

        if (void* symbols = ReadWholeFile("resource/fonts/DejaVuSans.ttf", fileSize))
        {
            ImFontConfig config;
            config.MergeMode = true;
            io.Fonts->AddFontFromMemoryTTF(symbols, fileSize, fontSize, &config, symbolRanges);
        }
    };

    addFont(size);
    // a second, bigger size for headings and icons, since a scaled up font looks blurry
    addFont(std::round(size * 1.4f));
    if (titleScale > 0.0f)
        addFont(std::round(size * titleScale));
}

// ImGui speaks UTF-8, VGUI's clipboard speaks wchar_t
static void SetClipboard(void*, const char* text)
{
    std::vector<wchar_t> wide(strlen(text) + 1);
    V_UTF8ToUnicode(text, wide.data(), static_cast<int>(wide.size() * sizeof(wchar_t)));
    system()->SetClipboardText(wide.data(), static_cast<int>(wcslen(wide.data())));
}

static const char* GetClipboard(void*)
{
    static std::string utf8;
    utf8.clear();

    int count = system()->GetClipboardTextCount();
    if (count <= 0)
        return utf8.c_str();

    std::vector<wchar_t> wide(count + 1);
    int length = system()->GetClipboardText(0, wide.data(), static_cast<int>(count * sizeof(wchar_t)));
    wide[std::clamp(length, 0, count)] = L'\0';

    utf8.resize(wcslen(wide.data()) * 4 + 1);
    V_UnicodeToUTF8(wide.data(), utf8.data(), static_cast<int>(utf8.size()));
    utf8.resize(strlen(utf8.c_str()));
    return utf8.c_str();
}

static int ToImGuiMouseButton(MouseCode code)
{
    switch (code)
    {
        case MOUSE_LEFT:   return ImGuiMouseButton_Left;
        case MOUSE_RIGHT:  return ImGuiMouseButton_Right;
        case MOUSE_MIDDLE: return ImGuiMouseButton_Middle;
        default:           return -1;
    }
}

CImGuiPanel::CImGuiPanel(const char* layoutFile, float titleFontScale) : BaseClass(nullptr, "ImGuiPanel"), m_flTitleFontScale(titleFontScale), m_pszLayoutFile(layoutFile)
{
    MakePopup();
    SetKeyBoardInputEnabled(true);
    SetMouseInputEnabled(true);
    SetPaintBackgroundEnabled(false);

    int wide, tall;
    surface()->GetScreenSize(wide, tall);
    SetBounds(0, 0, wide, tall);

    // CreateContext only makes the new context current when there is none yet, and the
    // console and the server browser each have their own
    m_pContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_pContext);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.SetClipboardTextFn = SetClipboard;
    io.GetClipboardTextFn = GetClipboard;
    ApplyNextClientTheme(ImGui::GetStyle());
    std::copy(std::begin(ImGui::GetStyle().Colors), std::end(ImGui::GetStyle().Colors), m_BaseColors);
    LoadFonts(io, m_flFontSize, m_flTitleFontScale);
    ImGui_ImplOpenGL2_Init();

    int layoutSize;
    if (m_pszLayoutFile)
    {
        if (void* layout = ReadWholeFile(m_pszLayoutFile, layoutSize))
        {
            ImGui::LoadIniSettingsFromMemory(static_cast<const char*>(layout), layoutSize);
            IM_FREE(layout);
        }
    }
}

std::string CImGuiPanel::Localized(const char* token)
{
    std::string utf8;
    if (const wchar_t* wide = g_pVGuiLocalize->Find(token))
    {
        utf8.resize(wcslen(wide) * 4 + 1);
        V_UnicodeToUTF8(wide, utf8.data(), static_cast<int>(utf8.size()));
        utf8.resize(strlen(utf8.c_str()));
    }

    return utf8;
}

std::string CImGuiPanel::Localized(const char* token, const char* english)
{
    std::string text = Localized(token);
    return text.empty() ? english : text;
}

ImFont* CImGuiPanel::TitleFont()
{
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    return atlas->Fonts.Size > 2 ? atlas->Fonts[2] : HeadingFont();
}

ImFont* CImGuiPanel::HeadingFont()
{
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    return atlas->Fonts.Size > 1 ? atlas->Fonts[1] : nullptr;
}

void CImGuiPanel::SetFontSize(float size)
{
    m_flPendingFontSize = size;
}

void CImGuiPanel::SetAppearance(ImGuiAppearance::Element element)
{
    m_Appearance = element;
}

void CImGuiPanel::MakePreview()
{
    m_bPreview = true;
    SetVisible(false);
    SetMouseInputEnabled(false);
    SetKeyBoardInputEnabled(false);

    // a preview shows the window where and how big it starts out, not where the player left the real one
    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(m_pContext);
    ImGui::ClearIniSettings();
    ImGui::SetCurrentContext(previous);

    PreparePreview();
}

ImDrawData* CImGuiPanel::RenderPreview(const ImGuiAppearance::Values& values, ImVec2& windowsMin, ImVec2& windowsMax)
{
    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(m_pContext);

    ApplyColors(values);
    if (values.fontSize != m_flFontSize || !m_iFontTextureID)
    {
        m_flFontSize = values.fontSize;
        ImGui::GetIO().Fonts->Clear();
        LoadFonts(ImGui::GetIO(), m_flFontSize, m_flTitleFontScale);
        CreateFontTexture();
    }

    // laid out for the real screen, so the window comes out where and as big as it would be
    ImGuiIO& io = ImGui::GetIO();
    int wide, tall;
    surface()->GetScreenSize(wide, tall);
    io.DisplaySize = ImVec2(static_cast<float>(wide), static_cast<float>(tall));
    io.DeltaTime = 1.0f / 60.0f;
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);

    KeepWindowsOnScreen();
    ImGui::NewFrame();
    DrawImGui();
    ImGui::Render();

    windowsMin = ImVec2(FLT_MAX, FLT_MAX);
    windowsMax = ImVec2(-FLT_MAX, -FLT_MAX);
    for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
    {
        if (!window->Active || (window->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_Popup)))
            continue;
        if (!strncmp(window->Name, "##Backdrop", 10))
            continue;

        windowsMin = ImMin(windowsMin, window->Pos);
        windowsMax = ImMax(windowsMax, ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y));
    }
    if (windowsMin.x > windowsMax.x)
    {
        windowsMin = ImVec2(0, 0);
        windowsMax = io.DisplaySize;
    }

    ImDrawData* data = ImGui::GetDrawData();
    ImGui::SetCurrentContext(previous);
    return data;
}

static void RenderPreviewCallback(const ImDrawList* parentList, const ImDrawCmd* command)
{
    ImGui_ImplOpenGL2_RenderDrawData(static_cast<ImDrawData*>(command->UserCallbackData));

    // the backend points GL at a list's vertices once, before its commands, so the rest of the
    // list this was called from would be drawn from the preview's; the reset callback after this
    // turns the arrays back on, which the nested call turned off
    const ImDrawVert* vertices = parentList->VtxBuffer.Data;
    glVertexPointer(2, GL_FLOAT, sizeof(ImDrawVert), reinterpret_cast<const char*>(vertices) + offsetof(ImDrawVert, pos));
    glTexCoordPointer(2, GL_FLOAT, sizeof(ImDrawVert), reinterpret_cast<const char*>(vertices) + offsetof(ImDrawVert, uv));
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ImDrawVert), reinterpret_cast<const char*>(vertices) + offsetof(ImDrawVert, col));
}

void CImGuiPanel::AddScaledDrawData(ImDrawList* drawList, ImDrawData* data, const ImVec2& offset, float scale, const ImVec4& clip)
{
    if (!data || !data->Valid)
        return;

    // the frame is made again for every preview, so it can be moved where it goes in place
    for (ImDrawList* list : data->CmdLists)
    {
        for (ImDrawVert& vertex : list->VtxBuffer)
            vertex.pos = ImVec2(offset.x + vertex.pos.x * scale, offset.y + vertex.pos.y * scale);

        for (ImDrawCmd& command : list->CmdBuffer)
        {
            ImVec4& rect = command.ClipRect;
            rect = ImVec4(offset.x + rect.x * scale, offset.y + rect.y * scale, offset.x + rect.z * scale, offset.y + rect.w * scale);
            rect = ImVec4(std::max(rect.x, clip.x), std::max(rect.y, clip.y), std::min(rect.z, clip.z), std::min(rect.w, clip.w));
        }
    }

    // in the screen's space now, like the frame it's drawn in
    data->DisplayPos = ImVec2(0, 0);
    data->DisplaySize = ImGui::GetIO().DisplaySize;
    data->FramebufferScale = ImVec2(1, 1);

    drawList->AddCallback(RenderPreviewCallback, data);
    drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

void CImGuiPanel::ApplyAppearance()
{
    if (m_Appearance == ImGuiAppearance::Element::Count)
        return;

    ImGuiAppearance::Values values = ImGuiAppearance::Current(m_Appearance);
    ApplyColors(values);
    if (values.fontSize != m_flFontSize)
        m_flPendingFontSize = values.fontSize;
}

void CImGuiPanel::ApplyColors(const ImGuiAppearance::Values& values)
{
    // the theme is made again only when the palette changes, the opacity goes on top every frame
    if (memcmp(&values.palette, &m_Palette, sizeof(m_Palette)) != 0)
    {
        m_Palette = values.palette;
        ApplyThemeColors(m_BaseColors, m_Palette);
        ApplyThemeColors(ImGui::GetStyle().Colors, m_Palette);
    }
    ImGuiAppearance::ApplyOpacity(ImGui::GetStyle(), m_BaseColors, values.opacity);
}

void CImGuiPanel::SaveLayout()
{
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantSaveIniSettings)
        return;

    io.WantSaveIniSettings = false;
    if (!m_pszLayoutFile)
        return;

    size_t size;
    const char* layout = ImGui::SaveIniSettingsToMemory(&size);

    FileHandle_t file = g_pFullFileSystem->Open(m_pszLayoutFile, "wb");
    if (file == FILESYSTEM_INVALID_HANDLE)
        return;

    g_pFullFileSystem->Write(layout, static_cast<int>(size), file);
    g_pFullFileSystem->Close(file);
}

CImGuiPanel::~CImGuiPanel()
{
    ImGui::SetCurrentContext(m_pContext);
    ImGui_ImplOpenGL2_Shutdown();
    ImGui::DestroyContext(m_pContext);
}

void CImGuiPanel::CreateFontTexture()
{
    ImGuiIO& io = ImGui::GetIO();

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // GoldSrc hands out GL texture names from its own counter instead of glGenTextures,
    // so a name from glGenTextures could later be reused (and overwritten) by the engine;
    // a font size change uploads into the name it already has
    if (!m_iFontTextureID)
        m_iFontTextureID = surface()->CreateNewTextureID();

    GLint lastTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastTexture);
    glBindTexture(GL_TEXTURE_2D, m_iFontTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, lastTexture);

    io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(m_iFontTextureID)));
}

void CImGuiPanel::Paint()
{
    ImGui::SetCurrentContext(m_pContext);
    ApplyAppearance();

    if (m_flPendingFontSize > 0.0f && m_flPendingFontSize != m_flFontSize)
    {
        m_flFontSize = m_flPendingFontSize;
        ImGui::GetIO().Fonts->Clear();
        LoadFonts(ImGui::GetIO(), m_flFontSize, m_flTitleFontScale);
        CreateFontTexture();
    }

    // ImGui_ImplOpenGL2_NewFrame is never called: all it does is create the font
    // texture with glGenTextures, which CreateFontTexture replaces
    if (!m_iFontTextureID)
        CreateFontTexture();

    ImGuiIO& io = ImGui::GetIO();

    int wide, tall;
    surface()->GetScreenSize(wide, tall);
    io.DisplaySize = ImVec2(static_cast<float>(wide), static_cast<float>(tall));

    double now = system()->GetCurrentTime();
    io.DeltaTime = m_flLastFrameTime > 0.0 ? std::max(static_cast<float>(now - m_flLastFrameTime), 0.0001f) : 1.0f / 60.0f;
    m_flLastFrameTime = now;

    ReleaseKeysLetGoElsewhere();
    KeepWindowsOnScreen();

    ImGui::NewFrame();
    DrawImGui();
    ImGui::Render();
    FitToWindows();
    SaveLayout();

    // the engine batches VGUI text and would draw it over us on the next flush
    surface()->DrawFlushText();

    // the backend saves and restores the rest of the GL state, but not alpha testing,
    // which the engine leaves on and which would cut off ImGui's antialiased edges
    GLboolean alphaTest = glIsEnabled(GL_ALPHA_TEST);
    glDisable(GL_ALPHA_TEST);
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    if (alphaTest)
        glEnable(GL_ALPHA_TEST);
}

ImVec2 CImGuiPanel::OnScreen(const ImVec2& size)
{
    constexpr float kMargin = 8.0f;
    ImVec2 screen = ImGui::GetIO().DisplaySize;
    return ImVec2(std::min(size.x, std::max(1.0f, screen.x - kMargin * 2.0f)), std::min(size.y, std::max(1.0f, screen.y - kMargin * 2.0f)));
}

void CImGuiPanel::KeepWindowsOnScreen()
{
    // no window may be dragged, resized or grown even partly off the screen; one bigger than the
    // screen, saved at a higher resolution, shrinks to fit it, and one that can't keeps its top
    // left corner on it, where the title bar is
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImVec2 screen = g.IO.DisplaySize;
    for (ImGuiWindow* window : g.Windows)
    {
        if (!window->WasActive || (window->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Tooltip)))
            continue;

        if ((window->Size.x > screen.x || window->Size.y > screen.y) && !(window->Flags & ImGuiWindowFlags_AlwaysAutoResize))
            ImGui::SetWindowSize(window, ImMin(window->Size, screen));

        ImVec2 pos = window->Pos;
        pos.x = std::max(0.0f, std::min(pos.x, screen.x - window->Size.x));
        pos.y = std::max(0.0f, std::min(pos.y, screen.y - window->Size.y));
        if (pos.x != window->Pos.x || pos.y != window->Pos.y)
            ImGui::SetWindowPos(window, pos);
    }
}

void CImGuiPanel::FitToWindows()
{
    // the panel only covers the ImGui windows, so clicks around them still reach the menu
    ImRect bounds;
    bool any = false;
    for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
    {
        if (!window->Active || window->Hidden)
            continue;

        if (any)
            bounds.Add(window->Rect());
        else
            bounds = window->Rect();
        any = true;
    }

    // a panel of zero size isn't painted, and then ImGui would never draw again
    if (!any)
        return;

    int x = static_cast<int>(std::floor(bounds.Min.x));
    int y = static_cast<int>(std::floor(bounds.Min.y));
    SetBounds(x, y, static_cast<int>(std::ceil(bounds.Max.x)) - x, static_cast<int>(std::ceil(bounds.Max.y)) - y);
}

void CImGuiPanel::ReleaseKeysLetGoElsewhere()
{
    // VGUI tracks every key, while OnKeyCodeReleased only comes while this panel has the
    // focus: an Enter let go after its command opened the loading dialog stayed held here,
    // and its repeat sent off whatever was typed next
    ImGuiIO& io = ImGui::GetIO();
    for (int code = KEY_FIRST + 1; code < KEY_LAST; code++)
    {
        ImGuiKey key = ToImGuiKey(static_cast<KeyCode>(code));
        if (key != ImGuiKey_None && ImGui::IsKeyDown(key) && !input()->IsKeyDown(static_cast<KeyCode>(code)))
            io.AddKeyEvent(key, false);
    }

    auto eitherDown = [](KeyCode left, KeyCode right) { return input()->IsKeyDown(left) || input()->IsKeyDown(right); };
    if (io.KeyCtrl && !eitherDown(KEY_LCONTROL, KEY_RCONTROL))
        io.AddKeyEvent(ImGuiMod_Ctrl, false);
    if (io.KeyShift && !eitherDown(KEY_LSHIFT, KEY_RSHIFT))
        io.AddKeyEvent(ImGuiMod_Shift, false);
    if (io.KeyAlt && !eitherDown(KEY_LALT, KEY_RALT))
        io.AddKeyEvent(ImGuiMod_Alt, false);
}

void CImGuiPanel::ResetInput()
{
    ImGui::SetCurrentContext(m_pContext);
    ImGui::GetIO().AddFocusEvent(false);
    ImGui::GetIO().AddFocusEvent(true);
}

void CImGuiPanel::OnSetFocus()
{
    BaseClass::OnSetFocus();

    ImGui::SetCurrentContext(m_pContext);
    ImGui::GetIO().AddFocusEvent(true);
}

void CImGuiPanel::OnKillFocus()
{
    BaseClass::OnKillFocus();

    // the release that would end a capture taken in OnMousePressed may never come here now
    if (input()->GetMouseCapture() == GetVPanel())
        input()->SetMouseCapture(0);

    // ImGui releases every key it thinks is held, or a Backspace let go elsewhere repeats forever
    ImGui::SetCurrentContext(m_pContext);
    ImGui::GetIO().AddFocusEvent(false);
}

void CImGuiPanel::OnCursorMoved(int x, int y)
{
    LocalToScreen(x, y);
    ImGui::SetCurrentContext(m_pContext);
    ImGui::GetIO().AddMousePosEvent(static_cast<float>(x), static_cast<float>(y));
}

void CImGuiPanel::OnMousePressed(MouseCode code)
{
    int button = ToImGuiMouseButton(code);
    if (button < 0)
        return;

    // keep getting cursor moves while a window is dragged past the panel's edge
    input()->SetMouseCapture(GetVPanel());

    ImGui::SetCurrentContext(m_pContext);
    ImGui::GetIO().AddMouseButtonEvent(button, true);
}

void CImGuiPanel::OnMouseDoublePressed(MouseCode code)
{
    // VGUI sends the second click of a double click only here
    OnMousePressed(code);
}

void CImGuiPanel::OnMouseReleased(MouseCode code)
{
    int button = ToImGuiMouseButton(code);
    if (button < 0)
        return;

    input()->SetMouseCapture(0);

    ImGui::SetCurrentContext(m_pContext);
    ImGui::GetIO().AddMouseButtonEvent(button, false);
}

void CImGuiPanel::OnCursorExited()
{
    ImGui::SetCurrentContext(m_pContext);
    ImGui::GetIO().AddMousePosEvent(-FLT_MAX, -FLT_MAX);
}

void CImGuiPanel::OnMouseWheeled(int delta)
{
    ImGui::SetCurrentContext(m_pContext);
    ImGui::GetIO().AddMouseWheelEvent(0.0f, static_cast<float>(delta));
}

void CImGuiPanel::OnKey(KeyCode code, bool down)
{
    ImGui::SetCurrentContext(m_pContext);
    ImGuiIO& io = ImGui::GetIO();

    if (code == KEY_LCONTROL || code == KEY_RCONTROL)
        io.AddKeyEvent(ImGuiMod_Ctrl, down);
    else if (code == KEY_LSHIFT || code == KEY_RSHIFT)
        io.AddKeyEvent(ImGuiMod_Shift, down);
    else if (code == KEY_LALT || code == KEY_RALT)
        io.AddKeyEvent(ImGuiMod_Alt, down);

    ImGuiKey key = ToImGuiKey(code);
    if (key != ImGuiKey_None)
        io.AddKeyEvent(key, down);
}

void CImGuiPanel::OnKeyCodePressed(KeyCode code)
{
    OnKey(code, true);
}

void CImGuiPanel::OnKeyCodeReleased(KeyCode code)
{
    OnKey(code, false);
}

void CImGuiPanel::OnKeyCodeTyped(KeyCode code)
{
    // ImGui already got the key in OnKeyCodePressed; the base class would hand it on to the parent panel
}

void CImGuiPanel::OnKeyTyped(wchar_t unichar)
{
    // VGUI also types control characters (backspace, enter...), which ImGui gets as keys
    if (unichar < 0x20 || unichar == 0x7F)
        return;

    ImGui::SetCurrentContext(m_pContext);
    ImGui::GetIO().AddInputCharacter(static_cast<unsigned int>(unichar));
}
