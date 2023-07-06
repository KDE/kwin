#include "xdgwmgestures_v1_interface.h"

#include "display.h"

#include "seat_interface.h"
#include "xdgshell_interface.h"

#include <qwayland-server-wm-gestures-v1.h>

namespace KWaylandServer
{
constexpr int version = 1;

class XdgWmGesturesV1Interface;

class XdgWmGesturesV1InterfacePrivate : public QtWaylandServer::xdg_wm_gestures_v1
{
public:
    XdgWmGesturesV1InterfacePrivate(XdgWmGesturesV1Interface *q, Display *display);

    XdgWmGesturesV1Interface *q;

protected:
    void xdg_wm_gestures_v1_destroy(Resource *resource) override;
    void xdg_wm_gestures_v1_action(Resource *resource, uint32_t serial, wl_resource *seat, struct wl_resource *toplevel, uint32_t action, uint32_t location) override;
};

XdgWmGesturesV1InterfacePrivate::XdgWmGesturesV1InterfacePrivate(XdgWmGesturesV1Interface *q, Display *display)
    : xdg_wm_gestures_v1(*display, version)
{
}

void XdgWmGesturesV1InterfacePrivate::xdg_wm_gestures_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void XdgWmGesturesV1InterfacePrivate::xdg_wm_gestures_v1_action(Resource *resource, uint32_t serial, wl_resource *seat, struct wl_resource *toplevel, uint32_t action, uint32_t location)
{
    auto seatInterface = SeatInterface::get(seat);
    if (!seatInterface) {
        wl_resource_post_error(resource->handle, 0, "Invalid seat");
        return;
    }
    auto toplevelInterface = XdgToplevelInterface::get(toplevel);
    if (!toplevelInterface) {
        wl_resource_post_error(resource->handle, 0, "Invalid surface");
        return;
    }
    Q_EMIT q->action({serial, seatInterface, toplevelInterface, XdgWmGesturesV1Interface::Location(location), XdgWmGesturesV1Interface::Action(action)});
}

XdgWmGesturesV1Interface::XdgWmGesturesV1Interface(KWaylandServer::Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<XdgWmGesturesV1InterfacePrivate>(this, display))
{
}

XdgWmGesturesV1Interface::~XdgWmGesturesV1Interface() = default;
}
