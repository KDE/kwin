#include "main.h"
#include "platform.h"
#include "wayland_server.h"

#include "wlcs_touch.h"
#include "util.h"

void thunk_touchDown(WlcsTouch* touch, wl_fixed_t x, wl_fixed_t y)
{
    KWin::kwin(touch)->touchDown(x, y);
}
void thunk_touchMove(WlcsTouch* touch, wl_fixed_t x, wl_fixed_t y)
{
    KWin::kwin(touch)->touchMove(x, y);
}
void thunk_touchUp(WlcsTouch* touch)
{
    KWin::kwin(touch)->touchUp();
}
void thunk_destroy(WlcsTouch* touch)
{
    KWin::kwin(touch)->destroy();
}

namespace KWin
{

WlcsTouch::WlcsTouch()
    : ::WlcsTouch
        { .version = 1
        , .touch_down = &thunk_touchDown
        , .touch_move = &thunk_touchMove
        , .touch_up = &thunk_touchUp
        , .destroy = &thunk_destroy
        }

{
}

void WlcsTouch::touchDown(wl_fixed_t x, wl_fixed_t y)
{
    kwinApp()->platform()->touchDown(0, into(x, y), timestamp++);
}

void WlcsTouch::touchMove(wl_fixed_t x, wl_fixed_t y)
{
    kwinApp()->platform()->touchMotion(0, into(x, y), timestamp++);
}

void WlcsTouch::touchUp()
{
    kwinApp()->platform()->touchUp(0, timestamp++);
}

void WlcsTouch::destroy()
{
    delete this;
}

};
