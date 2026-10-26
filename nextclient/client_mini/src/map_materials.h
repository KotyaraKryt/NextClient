#pragma once

#include <memory>
#include <vector>

namespace nitroapi
{
    class NitroApiInterface;
    class Unsubscriber;
} // namespace nitroapi

// Lets maps/<map>_materials.txt, which a server can send with its map, give the map's textures
// their materials. Its lines take precedence over sound/materials.txt until the map changes.
void MapMaterials_Subscribe(nitroapi::NitroApiInterface* nitro_api, std::vector<std::shared_ptr<nitroapi::Unsubscriber>>& unsub);
void MapMaterials_Init();
