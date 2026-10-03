#pragma once

#include "tier1/interface.h"

// The server's message of the day, drawn with ImGui instead of the client's HTML window.
// client_mini collects the MOTD messages and hands the text over when the client would show its window
class IMotdNext : public IBaseInterface
{
public:
    // false while motd_legacy asks for the client's own window
    virtual bool IsEnabled() = 0;
    // the text as the server sent it: plain text, HTML or a link
    virtual void Show(const char* text) = 0;
    virtual bool IsVisible() = 0;
    virtual void Hide() = 0;
};

#define MOTD_NEXT_INTERFACE_VERSION "MotdNext001"
