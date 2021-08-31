#pragma once

#include <wlcs/display_server.h>
#include <wlcs/pointer.h>

#include <QtCore>

namespace KWin
{

struct WlcsPointer : public ::WlcsPointer
{
    explicit WlcsPointer();

    quint32 timestamp = 1;

    void moveAbsolute(wl_fixed_t x, wl_fixed_t y);
    void moveRelative(wl_fixed_t dx, wl_fixed_t dy);
    void buttonUp(int button);
    void buttonDown(int button);
    void destroy();
};

inline WlcsPointer* kwin(::WlcsPointer* it)
{
    return static_cast<WlcsPointer*>(it);
}

inline const WlcsPointer* kwin(const ::WlcsPointer* it)
{
    return static_cast<const WlcsPointer*>(it);
}

};
