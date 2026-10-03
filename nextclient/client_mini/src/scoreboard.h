#pragma once

#include <memory>
#include <vector>

#include <nitroapi/NitroApiInterface.h>
#include <tier1/interface.h>

// The client's scoreboard gives way to the ImGui one in GameUI, which this feeds every frame it's up.
// Only on Linux for now: the hooks need client.dll's addresses on Windows
void ScoreboardSubscribe(nitroapi::ClientData* client_data, std::vector<std::shared_ptr<nitroapi::Unsubscriber>>& unsub);
void ScoreboardInit(CreateInterfaceFn gameui_factory);
void ScoreboardFrame(int intermission);
// a new map or server: the team scores start over
void ScoreboardReset();
void ScoreboardShutdown();
