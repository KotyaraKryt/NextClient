#include "HudSpeedometer.h"
#include "../main.h"

#include <cstdio>

HudSpeedometer::HudSpeedometer(nitroapi::NitroApiInterface* nitro_api) :
    HudBaseHelper(nitro_api)
{
}

void HudSpeedometer::Init()
{
    enabled_ = cl_enginefunc()->pfnRegisterVariable("hud_speedometer", "0", FCVAR_ARCHIVE);
    x_ = cl_enginefunc()->pfnRegisterVariable("hud_speedometer_x", "0.5", FCVAR_ARCHIVE);
    y_ = cl_enginefunc()->pfnRegisterVariable("hud_speedometer_y", "0.6", FCVAR_ARCHIVE);
    stayjump_ = cl_enginefunc()->pfnRegisterVariable("hud_speedometer_stayjump", "0", FCVAR_ARCHIVE);
}

void HudSpeedometer::Reset()
{
    takeoff_speed_ = 0.0f;
}

void HudSpeedometer::Draw(float flTime)
{
    if (enabled_->value == 0 || m_fPlayerDead)
    {
        return;
    }

    float speed = Vector(g_LastPlayerState.client.velocity).Length2D();

    bool on_ground = (g_LastPlayerState.client.flags & FL_ONGROUND) != 0;
    if (on_ground)
    {
        takeoff_speed_ = speed;
    }

    if (stayjump_->value != 0 && !on_ground)
    {
        speed = takeoff_speed_;
    }

    char text[16];
    snprintf(text, sizeof(text), "%.0f", speed);

    int w, h;
    GetScreenResolution(w, h);

    int x = static_cast<int>(w * x_->value) - DrawConsoleStringLen(text) / 2;
    int y = static_cast<int>(h * y_->value);

    DrawConsoleString(text, x, y);
}
