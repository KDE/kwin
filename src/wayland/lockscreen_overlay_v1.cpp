/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "lockscreen_overlay_v1.h"
#include "display.h"
#include "seat.h"
#include "surface.h"

#include "qwayland-server-kde-lockscreen-overlay-v1.h"

namespace KWin
{
static constexpr int s_version = 1;

class LockscreenOverlayV1InterfacePrivate : public QtWaylandServer::kde_lockscreen_overlay_v1
{
public:
    LockscreenOverlayV1InterfacePrivate(Display *display, LockscreenOverlayV1Interface *q)
        : QtWaylandServer::kde_lockscreen_overlay_v1(*display, s_version)
        , q(q)
    {
    }

protected:
    void kde_lockscreen_overlay_v1_allow(Resource *resource, struct ::wl_resource *surface) override
    {
        auto surfaceIface = SurfaceInterface::get(surface);
        if (surfaceIface->isMapped()) {
            return;
        }
        Q_EMIT q->allowRequested(surfaceIface);
    }
    void kde_lockscreen_overlay_v1_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

private:
    LockscreenOverlayV1Interface *const q;
};

LockscreenOverlayV1Interface::~LockscreenOverlayV1Interface() = default;

LockscreenOverlayV1Interface::LockscreenOverlayV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<LockscreenOverlayV1InterfacePrivate>(display, this))
{
}

}

#include "moc_lockscreen_overlay_v1.cpp"
