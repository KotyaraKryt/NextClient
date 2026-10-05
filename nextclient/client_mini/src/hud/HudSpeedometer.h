#pragma once

#include "HudBase.h"
#include "HudBaseHelper.h"

class HudSpeedometer : public HudBase, public HudBaseHelper
{
private:
    cvar_t* enabled_{};
    cvar_t* x_{};
    cvar_t* y_{};
    cvar_t* stayjump_{};

    float takeoff_speed_{};
public:
    explicit HudSpeedometer(nitroapi::NitroApiInterface* nitro_api);

    void Init() override;
    void Reset() override;
    void Draw(float flTime) override;
};
