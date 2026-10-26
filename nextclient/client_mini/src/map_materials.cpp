#include "map_materials.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <texture_materials/texture_materials.h>

#include "main.h"

namespace
{
    // COM_LoadFile allocates with Mem_Malloc for this value, the one the stock pmove uses
    constexpr int kMallocHunk = 5;

    texture_materials::MaterialTable g_Materials;
    std::string g_Level;
    std::string g_MaterialsPath;
    bool g_MaterialsFound{};

    // "maps/de_dust2.bsp" -> "maps/de_dust2_materials.txt"
    std::string MaterialsPath(std::string_view level)
    {
        if (level.ends_with(".bsp")) {
            level.remove_suffix(4);
        }
        return std::string(level) + "_materials.txt";
    }

    void LoadMaterials(const char* level)
    {
        g_Level = level;
        g_Materials.Clear();
        g_MaterialsPath.clear();
        g_MaterialsFound = false;

        if (g_Level.empty()) {
            return;
        }

        g_MaterialsPath = MaterialsPath(g_Level);

        int length = 0;
        uint8_t* file = gEngfuncs.COM_LoadFile(g_MaterialsPath.c_str(), kMallocHunk, &length);
        if (file == nullptr) {
            return;
        }

        g_Materials.Load(std::string_view(reinterpret_cast<const char*>(file), length));
        gEngfuncs.COM_FreeFile(file);
        g_MaterialsFound = true;
    }

    // Loads the table again when the level has changed since the last call
    const texture_materials::MaterialTable& CurrentMaterials()
    {
        const char* level = gEngfuncs.pfnGetLevelName();
        if (g_Level != level) {
            LoadMaterials(level);
        }
        return g_Materials;
    }

    void MapMaterials_f()
    {
        const texture_materials::MaterialTable& materials = CurrentMaterials();

        if (g_MaterialsPath.empty()) {
            gEngfuncs.Con_Printf("Not on a map.\n");
            return;
        }

        if (!g_MaterialsFound) {
            gEngfuncs.Con_Printf("%s not found, sound/materials.txt only.\n", g_MaterialsPath.c_str());
            return;
        }

        std::vector<std::pair<std::string, char>> entries(materials.get_materials().begin(), materials.get_materials().end());
        std::ranges::sort(entries);

        gEngfuncs.Con_Printf("%s: %d textures\n", g_MaterialsPath.c_str(), static_cast<int>(entries.size()));
        for (const auto& [texture, material] : entries) {
            gEngfuncs.Con_Printf("  %c %s\n", material, texture.c_str());
        }
    }
}

void MapMaterials_Subscribe(nitroapi::NitroApiInterface* nitro_api, std::vector<std::shared_ptr<nitroapi::Unsubscriber>>& unsub)
{
    nitroapi::ClientData* client_data = nitro_api->GetClientData();

    unsub.emplace_back(client_data->PM_FindTextureType |= [](char* name, const auto& next) {
        std::optional<char> material = CurrentMaterials().Find(name);
        return material ? *material : next->Invoke(name);
    });

    unsub.emplace_back(client_data->PM_CatagorizeTextureType += [] {
        std::optional<char> material = CurrentMaterials().Find(pmove->sztexturename);
        if (material) {
            pmove->chtexturetype = *material;
        }
    });
}

void MapMaterials_Init()
{
    gEngfuncs.pfnAddCommand("ncl_map_materials", MapMaterials_f);
}
