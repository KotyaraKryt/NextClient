#include "motd.h"
#include "parsemsg.h"

#include <next_gameui/IMotdNext.h>

#include <string>

namespace
{
    // the viewport's number for its MOTD window
    constexpr int kMenuIntro = 5;
    // the engine sends no more than 1536 bytes; this is only against a server that never says it's done
    constexpr size_t kMaxLength = 64 * 1024;

    IMotdNext* g_Motd = nullptr;
    nitroapi::ClientData* g_ClientData = nullptr;
    std::string g_Text;
    // the last part came, so the next MOTD message starts a new text
    bool g_bComplete = true;

    bool UseOurs()
    {
        return g_Motd && g_Motd->IsEnabled();
    }
}

void MotdSubscribe(nitroapi::ClientData* client_data, std::vector<std::shared_ptr<nitroapi::Unsubscriber>>& unsub)
{
    g_ClientData = client_data;

    // the MOTD comes in parts: whether it's the last one, then the text. Read before the client's
    // handler, which opens the window as soon as the last part is in
    unsub.emplace_back(client_data->UserMsg_MOTD |= [](const char* name, int size, void* data, const auto& next) {
        BEGIN_READ(data, size);
        bool last = READ_BYTE() != 0;
        const char* part = READ_STRING();

        if (g_bComplete)
            g_Text.clear();
        if (g_Text.size() < kMaxLength)
            g_Text += part;
        g_bComplete = last;

        return next->Invoke(name, size, data);
    });

    unsub.emplace_back(client_data->TeamFortressViewport__DisplayVGUIMenu |= [](void* viewport, int menu, const auto& next) {
        if (menu != kMenuIntro || !UseOurs() || g_Text.empty())
        {
            next->Invoke(viewport, menu);
            return;
        }

        g_Motd->Show(g_Text.c_str());
        // the viewport put up its dark panel for its own window; ours has a frame of its own
        g_ClientData->TeamFortressViewport__HideBackGround(viewport);
    });

    // the client waits for its MOTD window to close before it shows the team menu
    unsub.emplace_back(client_data->TeamFortressViewport__IsVGUIMenuActive |= [](void* viewport, int menu, const auto& next) {
        if (menu == kMenuIntro && g_Motd && g_Motd->IsVisible())
            return true;
        return next->Invoke(viewport, menu);
    });

    unsub.emplace_back(client_data->TeamFortressViewport__HideVGUIMenu += [](void* viewport, int menu) {
        if (menu == kMenuIntro && g_Motd)
            g_Motd->Hide();
    });
}

void MotdInit(CreateInterfaceFn gameui_factory)
{
    g_Motd = static_cast<IMotdNext*>(gameui_factory(MOTD_NEXT_INTERFACE_VERSION, nullptr));
}

void MotdShutdown()
{
    if (g_Motd)
        g_Motd->Hide();
    g_Motd = nullptr;
}
