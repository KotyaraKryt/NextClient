#pragma once

#include <memory>
#include <vector>

#include <nitroapi/NitroApiInterface.h>
#include <tier1/interface.h>

// The client's MOTD window gives way to the ImGui one in GameUI. Only on Linux for now:
// the viewport hooks need client.dll's addresses on Windows
void MotdSubscribe(nitroapi::ClientData* client_data, std::vector<std::shared_ptr<nitroapi::Unsubscriber>>& unsub);
void MotdInit(CreateInterfaceFn gameui_factory);
void MotdShutdown();
