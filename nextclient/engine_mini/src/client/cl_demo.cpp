#include "engine.h"

#include <cstring>
#include <format>
#include <string>

#include <optick.h>
#include <cvars/cvar_defaults.h>

#include "client/cl_demo.h"
#include "common/common.h"
#include "common/filesystem.h"

namespace
{
    constexpr char kDemoPathId[] = "GAMECONFIG";

    cvar_t* g_DemoFolderCvar{};
}

void CL_Stop_f()
{
    OPTICK_EVENT();

    eng()->CL_Stop_f.InvokeChained();
}

void CL_DemoInit()
{
    g_DemoFolderCvar = gEngfuncs.pfnRegisterVariable(cvars::kDemoFolder.name, cvars::kDemoFolder.value, FCVAR_ARCHIVE);
}

void CL_RedirectRecordToDemoFolder()
{
    const char* folder = g_DemoFolderCvar->string;
    if (Cmd_Argc() < 2 || folder[0] == '\0')
    {
        return;
    }

    const char* name = Cmd_Argv(1);
    if (std::strpbrk(name, "/\\") != nullptr)
    {
        return;
    }

    // CL_Record_f writes under GAMECONFIG and fails on a folder that doesn't exist yet
    FS_CreateDirHierarchy(folder, kDemoPathId);

    std::string command = std::format("record \"{}/{}\"", folder, name);
    for (int i = 2; i < Cmd_Argc(); i++)
    {
        command += std::format(" \"{}\"", Cmd_Argv(i));
    }

    eng()->Cmd_TokenizeString(command.c_str());
}
