#pragma once

#include <QPointF>

#include <wlcs/display_server.h>
#include <wlcs/touch.h>
#include <wlcs/pointer.h>

inline auto into(wl_fixed_t t)
{
    return wl_fixed_to_double(t);
}

inline auto into(wl_fixed_t x, wl_fixed_t y)
{
    return QPointF(into(x), into(y));
}