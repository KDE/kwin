#include "toplevel_tag_v1.h"
#include "display.h"
#include "xdgshell_p.h"

namespace KWin
{

static constexpr uint32_t s_version = 1;

ToplevelTagManagerV1::ToplevelTagManagerV1(Display *display, QObject *parent)
    : QObject(parent)
    , QtWaylandServer::xdg_toplevel_tag_manager_v1(*display, s_version)
{
}

void ToplevelTagManagerV1::xdg_toplevel_tag_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ToplevelTagManagerV1::xdg_toplevel_tag_manager_v1_set_toplevel_tag(Resource *resource, ::wl_resource *toplevel, const QString &tag)
{
    auto interface = XdgToplevelInterfacePrivate::get(toplevel);
    interface->windowTag = tag;
    Q_EMIT interface->q->tagChanged();
}

void ToplevelTagManagerV1::xdg_toplevel_tag_manager_v1_set_toplevel_description(Resource *resource, ::wl_resource *toplevel, const QString &description)
{
    auto interface = XdgToplevelInterfacePrivate::get(toplevel);
    interface->windowDescription = description;
    Q_EMIT interface->q->descriptionChanged();
}
}
