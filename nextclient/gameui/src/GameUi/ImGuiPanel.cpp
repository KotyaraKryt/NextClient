#include "ImGuiPanel.h"
#include "ImGuiTheme.h"

#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
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
#endif
#include <GL/gl.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
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

static void LoadFont(ImGuiIO& io, const char* path, float size)
{
    FileHandle_t file = g_pFullFileSystem->Open(path, "rb");
    if (file == FILESYSTEM_INVALID_HANDLE)
        return;

    int fileSize = g_pFullFileSystem->Size(file);
    if (fileSize <= 0)
    {
        g_pFullFileSystem->Close(file);
        return;
    }

    void* data = IM_ALLOC(fileSize);
    g_pFullFileSystem->Read(data, fileSize, file);
    g_pFullFileSystem->Close(file);

    // Latin and Cyrillic, plus the punctuation, arrows, box drawing and shapes that servers
    // and plugins like to decorate their messages with
    static const ImWchar ranges[] = {
        0x0020, 0x00FF, // Basic Latin, Latin-1
        0x0100, 0x017F, // Latin Extended-A
        0x0400, 0x052F, // Cyrillic
        0x2000, 0x206F, // General Punctuation
        0x2190, 0x21FF, // Arrows
        0x2500, 0x25FF, // Box Drawing, Block Elements, Geometric Shapes
        0x2600, 0x26FF, // Miscellaneous Symbols
        0,
    };

    // the atlas takes ownership of data and frees it with IM_FREE
    io.Fonts->AddFontFromMemoryTTF(data, fileSize, size, nullptr, ranges);
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

CImGuiPanel::CImGuiPanel() : BaseClass(nullptr, "ImGuiPanel")
{
    MakePopup();
    SetKeyBoardInputEnabled(true);
    SetMouseInputEnabled(true);
    SetPaintBackgroundEnabled(false);

    int wide, tall;
    surface()->GetScreenSize(wide, tall);
    SetBounds(0, 0, wide, tall);

    m_pContext = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.SetClipboardTextFn = SetClipboard;
    io.GetClipboardTextFn = GetClipboard;
    ApplyNextClientTheme(ImGui::GetStyle());
    LoadFont(io, "resource/fonts/JetBrainsMono-Regular.ttf", 16.0f);
    ImGui_ImplOpenGL2_Init();
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
    // so a name from glGenTextures could later be reused (and overwritten) by the engine
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

    ImGui::NewFrame();
    DrawImGui();
    ImGui::Render();
    FitToWindows();

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
