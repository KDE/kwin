#include "main.h"
#include "platform.h"
#include "wayland_server.h"

#include "wlcs_pointer.h"
#include "util.h"

void thunk_moveAbsolute(WlcsPointer* ptr, wl_fixed_t x, wl_fixed_t y)
{
    KWin::kwin(ptr)->moveAbsolute(x, y);
}
void thunk_moveRelative(WlcsPointer* ptr, wl_fixed_t dx, wl_fixed_t dy)
{
    KWin::kwin(ptr)->moveRelative(dx, dy);
}
void thunk_buttonUp(WlcsPointer* ptr, int button)
{
    KWin::kwin(ptr)->buttonUp(button);
}
void thunk_buttonDown(WlcsPointer* ptr, int button)
{
    KWin::kwin(ptr)->buttonDown(button);
}
void thunk_destroy(WlcsPointer* ptr)
{
    KWin::kwin(ptr)->destroy();
}

namespace KWin
{

WlcsPointer::WlcsPointer()
    : ::WlcsPointer
        { .version = 1
        , .move_absolute = &thunk_moveAbsolute
        , .move_relative = &thunk_moveRelative
        , .button_up = &thunk_buttonUp
        , .button_down = &thunk_buttonDown
        , .destroy = &thunk_destroy
        }
{
}

void WlcsPointer::moveAbsolute(wl_fixed_t x, wl_fixed_t y)
{
    kwinApp()->platform()->warpPointer(into(x, y));
}

void WlcsPointer::moveRelative(wl_fixed_t dx, wl_fixed_t dy)
{
    kwinApp()->platform()->pointerMotion(into(dx, dy), timestamp++);
}

void WlcsPointer::buttonUp(int button)
{
    kwinApp()->platform()->pointerButtonReleased(button, timestamp++);
}

void WlcsPointer::buttonDown(int button)
{
    kwinApp()->platform()->pointerButtonPressed(button, timestamp++);
}

void WlcsPointer::destroy()
{
    delete this;
}

};
