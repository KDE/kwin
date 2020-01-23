/********************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "qtsurfaceextension_interface.h"
#include "global_p.h"
#include "resource_p.h"
#include "display.h"
#include "surface_interface.h"

#include <QTimer>
#include <QVariant>

#include <wayland-server.h>
#include <wayland-qt-surface-extension-server-protocol.h>

namespace KWayland
{
namespace Server
{

class QtSurfaceExtensionInterface::Private : public Global::Private
{
public:
    Private(QtSurfaceExtensionInterface *q, Display *d);

    QList<QtExtendedSurfaceInterface*> surfaces;

private:
    static void createSurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface);
    void bind(wl_client *client, uint32_t version, uint32_t id) override;
    void createSurface(wl_client *client, uint32_t version, uint32_t id, SurfaceInterface *surface, wl_resource *parentResource);

    QtSurfaceExtensionInterface *q;
    static const struct qt_surface_extension_interface s_interface;
    static const quint32 s_version;
};

const quint32 QtSurfaceExtensionInterface::Private::s_version = 1;

QtSurfaceExtensionInterface::Private::Private(QtSurfaceExtensionInterface *q, Display *d)
    : Global::Private(d, &qt_surface_extension_interface, s_version)
    , q(q)
{
}

#ifndef K_DOXYGEN
const struct qt_surface_extension_interface QtSurfaceExtensionInterface::Private::s_interface = {
    createSurfaceCallback
};
#endif


class QtExtendedSurfaceInterface::Private : public Resource::Private
{
public:
    Private(QtExtendedSurfaceInterface *q, QtSurfaceExtensionInterface *shell, SurfaceInterface *surface, wl_resource *parentResource);

    SurfaceInterface *surface;

private:
    // interface callbacks
    static void updateGenericPropertyCallback(wl_client *client, wl_resource *resource, const char *name, wl_array *value);
    static void setContentOrientationMaskCallback(wl_client *client, wl_resource *resource, int32_t orientation);
    static void setWindowFlagsCallback(wl_client *client, wl_resource *resource, int32_t flags);
    static void raiseCallback(wl_client *client, wl_resource *resource);
    static void lowerCallback(wl_client *client, wl_resource *resource);

    QtExtendedSurfaceInterface *q_func() {
        return reinterpret_cast<QtExtendedSurfaceInterface *>(q);
    }

    static const struct qt_extended_surface_interface s_interface;
};

QtSurfaceExtensionInterface::QtSurfaceExtensionInterface(Display *display, QObject *parent)
    : Global(new Private(this, display), parent)
{
}

QtSurfaceExtensionInterface::~QtSurfaceExtensionInterface() = default;

void QtSurfaceExtensionInterface::Private::bind(wl_client *client, uint32_t version, uint32_t id)
{
    auto c = display->getConnection(client);
    wl_resource *shell = c->createResource(&qt_surface_extension_interface, qMin(version, s_version), id);
    if (!shell) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(shell, &s_interface, this, nullptr);
}

void QtSurfaceExtensionInterface::Private::createSurfaceCallback(wl_client *client, wl_resource *resource, uint32_t id, wl_resource *surface)
{
    auto s = reinterpret_cast<QtSurfaceExtensionInterface::Private*>(wl_resource_get_user_data(resource));
    s->createSurface(client, wl_resource_get_version(resource), id, SurfaceInterface::get(surface), resource);
}

void QtSurfaceExtensionInterface::Private::createSurface(wl_client *client, uint32_t version, uint32_t id, SurfaceInterface *surface, wl_resource *parentResource)
{
    auto it = std::find_if(surfaces.constBegin(), surfaces.constEnd(),
        [surface](QtExtendedSurfaceInterface *s) {
            return surface == s->surface();
        }
    );
    if (it != surfaces.constEnd()) {
        wl_resource_post_error(surface->resource(), WL_DISPLAY_ERROR_INVALID_OBJECT, "Qt Surface Extension already created");
        return;
    }
    QtExtendedSurfaceInterface *shellSurface = new QtExtendedSurfaceInterface(q, surface, parentResource);
    surfaces << shellSurface;
    QObject::connect(shellSurface, &QtExtendedSurfaceInterface::destroyed, q,
        [this, shellSurface] {
            surfaces.removeAll(shellSurface);
        }
    );
    shellSurface->d->create(display->getConnection(client), version, id);
    emit q->surfaceCreated(shellSurface);
}

/*********************************
 * ShellSurfaceInterface
 *********************************/
QtExtendedSurfaceInterface::Private::Private(QtExtendedSurfaceInterface *q, QtSurfaceExtensionInterface *shell, SurfaceInterface *surface, wl_resource *parentResource)
    : Resource::Private(q, shell, parentResource, &qt_extended_surface_interface, &s_interface)
    , surface(surface)
{
}

#ifndef K_DOXYGEN
const struct qt_extended_surface_interface QtExtendedSurfaceInterface::Private::s_interface = {
    updateGenericPropertyCallback,
    setContentOrientationMaskCallback,
    setWindowFlagsCallback,
    raiseCallback,
    lowerCallback
};
#endif

void QtExtendedSurfaceInterface::Private::lowerCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    emit cast<Private>(resource)->q_func()->lowerRequested();
}

void QtExtendedSurfaceInterface::Private::raiseCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client)
    emit cast<Private>(resource)->q_func()->raiseRequested();
}

void QtExtendedSurfaceInterface::Private::setContentOrientationMaskCallback(wl_client *client, wl_resource *resource, int32_t orientation)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    Q_UNUSED(orientation)
}

void QtExtendedSurfaceInterface::Private::setWindowFlagsCallback(wl_client *client, wl_resource *resource, int32_t flags)
{
    Q_UNUSED(client)
    Q_UNUSED(resource)
    Q_UNUSED(flags)
}

void QtExtendedSurfaceInterface::Private::updateGenericPropertyCallback(wl_client *client, wl_resource *resource, const char *name, wl_array *value)
{
    Q_UNUSED(client)
    QByteArray data = QByteArray::fromRawData(static_cast<char *>(value->data), value->size);

    QVariant variantValue;
    QDataStream ds(data);
    ds >> variantValue;
    cast<Private>(resource)->q_func()->setProperty(name, variantValue);
}

QtExtendedSurfaceInterface::QtExtendedSurfaceInterface(QtSurfaceExtensionInterface *shell, SurfaceInterface *parent, wl_resource *parentResource)
    : Resource(new Private(this, shell, parent, parentResource))
{
    auto unsetSurface = [this] {
        Q_D();
        d->surface = nullptr;
    };
    connect(parent, &Resource::unbound, this, unsetSurface);
    connect(parent, &QObject::destroyed, this, unsetSurface);
}

QtExtendedSurfaceInterface::~QtExtendedSurfaceInterface() = default;

SurfaceInterface *QtExtendedSurfaceInterface::surface() const
{
    Q_D();
    return d->surface;
}

QtSurfaceExtensionInterface *QtExtendedSurfaceInterface::shell() const
{
    Q_D();
    return reinterpret_cast<QtSurfaceExtensionInterface*>(d->global);
}

QtExtendedSurfaceInterface::Private *QtExtendedSurfaceInterface::d_func() const
{
    return reinterpret_cast<QtExtendedSurfaceInterface::Private*>(d.data());
}

void QtExtendedSurfaceInterface::close()
{
    Q_D();
    if (!d->resource) {
        return;
    }
    qt_extended_surface_send_close(d->resource);
    d->client->flush();
}

}
}
