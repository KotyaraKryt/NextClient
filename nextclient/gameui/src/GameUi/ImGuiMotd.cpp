#include "ImGuiMotd.h"
#include "GameUi.h"
#include "ImGuiForm.h"

#include <vgui/ISurfaceNext.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/HTML.h>

#include <FileSystem.h>
#include <cvardef.h>

#include <imgui/imgui.h>
#include <steam/steam_api.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifndef _WIN32
#include <spawn.h>
#include <sys/wait.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#ifndef _WIN32
extern char** environ;
#endif

using namespace ImGuiForm;

namespace
{
    cvar_t* g_pLegacyCvar = nullptr;

    const ImVec4 kLinkColor(0.45f, 0.68f, 1.0f, 1.0f);

    void OpenUrl(const std::string& url)
    {
#ifdef _WIN32
        vgui2::system()->ShellExecute("open", url.c_str());
#else
        // the game's own libraries mustn't leak into the browser; sh puts xdg-open in the background
        // and is gone at once, so waiting for it doesn't hold the game up
        std::vector<std::string> env;
        for (char** var = environ; *var; var++)
        {
            if (strncmp(*var, "LD_PRELOAD=", 11) != 0 && strncmp(*var, "LD_LIBRARY_PATH=", 16) != 0)
                env.emplace_back(*var);
        }

        std::vector<char*> envp;
        for (std::string& var : env)
            envp.push_back(var.data());
        envp.push_back(nullptr);

        const char* argv[] = { "sh", "-c", "xdg-open \"$1\" >/dev/null 2>&1 &", "sh", url.c_str(), nullptr };
        pid_t pid;
        if (posix_spawn(&pid, "/bin/sh", nullptr, nullptr, const_cast<char**>(argv), envp.data()) == 0)
            waitpid(pid, nullptr, 0);
#endif
    }

    // the MOTD in a file of the mod's folder, where the client puts its own: the browser runs in
    // Steam's process, which may not see the system's temporary folders. "" if it couldn't be written
    std::string WriteTempHtml(const std::string& html)
    {
        constexpr const char* kFileName = "motd_next.html";

        FileHandle_t file = g_pFullFileSystem->Open(kFileName, "wb", "GAMECONFIG");
        if (!file)
            return "";
        g_pFullFileSystem->Write(html.data(), static_cast<int>(html.size()), file);
        g_pFullFileSystem->Close(file);

        char path[512];
        if (!g_pFullFileSystem->GetLocalPath(kFileName, path, sizeof(path)))
            return "";
        return path;
    }

    // file:///C:/... on Windows, file:///home/... elsewhere
    std::string FileUrl(std::string path)
    {
        std::replace(path.begin(), path.end(), '\\', '/');
        return path[0] == '/' ? "file://" + path : "file:///" + path;
    }

    // tricks against old browsers, which the client refuses to show as well
    bool IsHostile(const std::string& html)
    {
        return html.find("img src=\"view-source:") != std::string::npos || html.find("<style>;@/*") != std::string::npos;
    }

    // the text of a link that opens it when clicked
    bool Link(const std::string& url)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kLinkColor);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(url.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        if (!ImGui::IsItemHovered())
            return false;

        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, max.y), max, ImGui::GetColorU32(kLinkColor));
        return ImGui::IsItemClicked(ImGuiMouseButton_Left);
    }
}

CImGuiMotd::CImGuiMotd()
{
    SetAppearance(ImGuiAppearance::Element::Motd);
    SetVisible(false);
}

void CImGuiMotd::Show(const char* text)
{
    m_Raw = text;
    m_Text = ParseMotd(m_Raw);
    m_flBodyHeight = 0.0f;
    m_bPage = (!m_Text.url.empty() || m_Text.html) && !IsHostile(m_Raw);

    if (m_bPage)
        LoadPage();
    else if (m_pHtml)
        m_pHtml->SetVisible(false);

    SetVisible(true);
    MoveToFront();
    RequestFocus();
    ResetInput();
    m_bFocusWindow = true;
}

void CImGuiMotd::PreparePreview()
{
    m_Raw = "<h1>Welcome!</h1><p>Be polite, play fair and have fun.</p><ul><li>No cheats</li><li>No spam in the chat</li><li>Listen to the admins</li></ul><p>Our site: https://example.com</p>";
    m_Text = ParseMotd(m_Raw);
    m_bPage = false;
}

void CImGuiMotd::OnThink()
{
    BaseClass::OnThink();

    // left the server with the MOTD still up
    if (IsVisible() && !GameUI().IsInLevel())
        Close();
}

void CImGuiMotd::Close()
{
    SetVisible(false);

    // so a video or music on the page doesn't play on behind the game
    if (m_pHtml)
        m_pHtml->OpenURL("about:blank", nullptr);
}

void CImGuiMotd::LoadPage()
{
    if (!m_pHtml)
    {
        m_pHtml = new vgui2::HTML(this, "MotdHTML");
        m_pHtml->InitializeBrowser("Valve Client", true);
        m_pHtml->SetContextMenuEnabled(false);

        if (SteamApps())
        {
            std::vector<std::string> languages;
            std::string available = SteamApps()->GetAvailableGameLanguages();
            size_t begin = 0;
            while (begin <= available.size())
            {
                size_t end = available.find(',', begin);
                if (end == std::string::npos)
                    end = available.size();
                if (end > begin)
                    languages.push_back(available.substr(begin, end - begin));
                begin = end + 1;
            }
            m_pHtml->SetLangSettings(SteamApps()->GetCurrentGameLanguage(), languages);
        }
    }

    m_pHtml->SetVisible(true);

    if (!m_Text.url.empty() && !m_Text.html)
    {
        m_pHtml->OpenURL(m_Text.url.c_str(), nullptr);
        return;
    }

    // what the client does with an HTML MOTD as well: the page from a file, so its frames and links work
    std::string path = WriteTempHtml(m_Raw);
    if (!path.empty())
        m_pHtml->OpenURL(FileUrl(path).c_str(), nullptr);
}

void CImGuiMotd::PlacePage(const ImVec2& min, const ImVec2& max)
{
    int x, y;
    GetPos(x, y);
    int bounds[4] = { static_cast<int>(min.x) - x, static_cast<int>(min.y) - y, static_cast<int>(max.x - min.x), static_cast<int>(max.y - min.y) };

    // every move or resize makes the browser lay the page out and send it all again
    int current[4];
    m_pHtml->GetBounds(current[0], current[1], current[2], current[3]);
    if (memcmp(bounds, current, sizeof(bounds)) != 0)
        m_pHtml->SetBounds(bounds[0], bounds[1], bounds[2], bounds[3]);
}

void CImGuiMotd::OpenInBrowser()
{
    if (!m_Text.url.empty())
    {
        OpenUrl(m_Text.url);
        return;
    }

    std::string path = WriteTempHtml(m_Raw);
    if (path.empty())
        return;

    OpenUrl(FileUrl(path));
}

void CImGuiMotd::DrawBody()
{
    const ImGuiStyle& style = ImGui::GetStyle();

    if (m_Text.blocks.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::PushTextWrapPos(0.0f);
        if (!m_Text.url.empty())
            ImGui::TextUnformatted(Localized("#GameUI_MotdWebPage", "The server shows its message on a web page:").c_str());
        else
            ImGui::TextUnformatted(Localized("#GameUI_MotdNoText", "The message has no text to show here.").c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        if (!m_Text.url.empty())
        {
            ImGui::Dummy(ImVec2(0, style.ItemSpacing.y));
            if (Link(m_Text.url))
                OpenUrl(m_Text.url);
        }
        return;
    }

    ImGui::PushTextWrapPos(0.0f);
    for (size_t i = 0; i < m_Text.blocks.size(); i++)
    {
        const MotdText::Block& block = m_Text.blocks[i];
        if (i > 0)
            ImGui::Dummy(ImVec2(0, style.ItemSpacing.y));

        switch (block.kind)
        {
            case MotdText::Kind::Heading:
                ImGui::PushFont(HeadingFont());
                ImGui::TextUnformatted(block.text.c_str());
                ImGui::PopFont();
                break;
            case MotdText::Kind::ListItem:
                ImGui::Bullet();
                ImGui::SameLine();
                ImGui::TextUnformatted(block.text.c_str());
                break;
            case MotdText::Kind::Rule:
                ImGui::Separator();
                break;
            case MotdText::Kind::Paragraph:
                ImGui::TextUnformatted(block.text.c_str());
                break;
        }
    }
    ImGui::PopTextWrapPos();
}

void CImGuiMotd::DrawLinks()
{
    if (m_Text.links.empty())
        return;

    ImGui::Dummy(ImVec2(0, ImGui::GetStyle().ItemSpacing.y));
    ImGui::TextDisabled("%s", Localized("#GameUI_MotdLinks", "Links").c_str());
    for (const std::string& link : m_Text.links)
    {
        if (Link(link))
            OpenUrl(link);
    }
}

void CImGuiMotd::DrawImGui()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    DimmedBackdrop(m_WindowMin, m_WindowMax, ImGuiAppearance::Current(ImGuiAppearance::Element::Motd).dim);

    // a page is made for the client's window, which is about 900 wide
    float width = m_bPage ? std::clamp(viewport->Size.x * 0.62f, std::min(640.0f, viewport->Size.x - 32.0f), 1000.0f)
                          : std::clamp(viewport->Size.x * 0.5f, std::min(560.0f, viewport->Size.x - 32.0f), 860.0f);
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    if (m_bFocusWindow)
    {
        ImGui::SetNextWindowFocus();
        m_bFocusWindow = false;
    }

    const char* host = GameClientExports() ? GameClientExports()->GetServerHostName() : nullptr;
    std::string title = (host && *host ? std::string(host) : Localized("#GameUI_MotdTitle", "Message of the day")) + "###Motd";

    bool close = false;
    bool open = true;
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin(title.c_str(), &open, flags))
    {
        const ImGuiStyle& style = ImGui::GetStyle();

        constexpr float kBodyPadding = 14.0f;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kBodyPadding, kBodyPadding));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kRounding);
        if (m_bPage && m_pHtml)
        {
            // the browser goes over the dark panel, a pixel in so its border shows
            float height = std::clamp(viewport->Size.y * 0.6f, 280.0f, 620.0f);
            ImGui::BeginChild("Body", ImVec2(0, height), true, ImGuiWindowFlags_NoScrollbar);
            ImVec2 min = ImGui::GetWindowPos();
            ImVec2 max(min.x + ImGui::GetWindowWidth(), min.y + ImGui::GetWindowHeight());

            // under the browser, seen until the page draws over it: some servers check the player first
            std::string loading = Localized("#GameUI_MotdLoading", "Loading the page...");
            ImVec2 textSize = ImGui::CalcTextSize(loading.c_str());
            ImGui::SetCursorScreenPos(ImVec2((min.x + max.x - textSize.x) * 0.5f, (min.y + max.y - textSize.y) * 0.5f));
            ImGui::TextDisabled("%s", loading.c_str());
            ImGui::EndChild();
            PlacePage(ImVec2(min.x + 1.0f, min.y + 1.0f), ImVec2(max.x - 1.0f, max.y - 1.0f));
        }
        else
        {
            // the text on the dark panel, scrolling once it's taller than most of the screen
            float maxHeight = std::max(160.0f, viewport->Size.y * 0.62f);
            float height = std::clamp(m_flBodyHeight + kBodyPadding * 2.0f, 96.0f, maxHeight);
            ImGui::BeginChild("Body", ImVec2(0, height), true);
            float top = ImGui::GetCursorPosY();
            DrawBody();
            DrawLinks();
            m_flBodyHeight = ImGui::GetCursorPosY() - top - style.ItemSpacing.y;
            ImGui::EndChild();
        }
        ImGui::PopStyleVar(2);
        ImGui::Dummy(ImVec2(0, style.ItemSpacing.y));

        std::string okText = Localized("#GameUI_OK");
        float okWidth = std::max(110.0f, ImGui::CalcTextSize(okText.c_str()).x + style.FramePadding.x * 2.0f);

        // the page itself, for what the text can't show: pictures, colours, a whole site
        if (m_bPage)
        {
            if (ImGui::Button(Localized("#GameUI_MotdOpenBrowser", "Open in browser").c_str()))
                OpenInBrowser();
            ImGui::SameLine();
        }

        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - okWidth);
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(ImGuiCol_CheckMark, 0.85f));
        close = ImGui::Button(okText.c_str(), ImVec2(okWidth, 0));
        ImGui::PopStyleColor(2);

        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)
            || ImGui::IsKeyPressed(ImGuiKey_Space))
            close = true;

        m_WindowMin = ImGui::GetWindowPos();
        m_WindowMax = ImVec2(m_WindowMin.x + ImGui::GetWindowWidth(), m_WindowMin.y + ImGui::GetWindowHeight());
    }
    ImGui::End();

    // the client sees it closed through IsVisible, and goes on to the team menu
    if (close || !open)
        Close();
}

static CMotdNext g_MotdNext;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CMotdNext, IMotdNext, MOTD_NEXT_INTERFACE_VERSION, g_MotdNext);

void CMotdNext::RegisterCvars()
{
    g_pLegacyCvar = engine->pfnRegisterVariable("motd_legacy", "0", FCVAR_ARCHIVE);
}

bool CMotdNext::IsEnabled()
{
    return g_pLegacyCvar && g_pLegacyCvar->value == 0.0f;
}

void CMotdNext::Show(const char* text)
{
    if (!m_hPanel.Get())
    {
        // under the root panel: the menu, and everything under it, is hidden while playing
        m_hPanel = vgui2::SETUP_PANEL(new CImGuiMotd());
        m_hPanel->SetParent(vgui2::surface()->GetEmbeddedPanel());
    }

    m_hPanel->Show(text);
}

bool CMotdNext::IsVisible()
{
    return m_hPanel.Get() && m_hPanel->IsVisible();
}

void CMotdNext::Hide()
{
    if (m_hPanel.Get() && m_hPanel->IsVisible())
        m_hPanel->Close();
}
