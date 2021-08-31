#pragma once

#include <wlcs/display_server.h>
#include <wlcs/touch.h>

#include <QtCore>

namespace KWin
{

struct WlcsTouch : public ::WlcsTouch
{
    explicit WlcsTouch();

    quint32 timestamp = 1;

    void touchDown(wl_fixed_t x, wl_fixed_t y);
    void touchMove(wl_fixed_t x, wl_fixed_t y);
    void touchUp();
    void destroy();
};

inline WlcsTouch* kwin(::WlcsTouch* it)
{
    return static_cast<WlcsTouch*>(it);
}

inline const WlcsTouch* kwin(const ::WlcsTouch* it)
{
    return static_cast<const WlcsTouch*>(it);
}

};
