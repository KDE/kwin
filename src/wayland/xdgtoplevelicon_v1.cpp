/*
    SPDX-FileCopyrightText: 2024 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xdgtoplevelicon_v1.h"
#include "core/graphicsbufferview.h"
#include "display.h"
#include "utils/resource.h"
#include "xdgshell.h"
#include "xdgshell_p.h"

#include <qwayland-server-xdg-toplevel-icon-v1.h>

#include <QDebug>
#include <QPointer>

#include <memory>

namespace KWin
{
static constexpr int version = 1;

class XdgToplevelIconV1Interface : public QtWaylandServer::xdg_toplevel_icon_v1
{
public:
    XdgToplevelIconV1Interface(::wl_resource *resource, Display *display)
        : xdg_toplevel_icon_v1(resource)
        , m_display(display)
    {
    }

    static XdgToplevelIconV1Interface *get(wl_resource *resource)
    {
        return resource_cast<XdgToplevelIconV1Interface *>(resource);
    }

    QIcon m_icon;

protected:
    void xdg_toplevel_icon_v1_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void xdg_toplevel_icon_v1_destroy_resource(Resource *resource) override
    {
        delete this;
    }

    void xdg_toplevel_icon_v1_set_name(Resource *resource, const QString &icon_name) override
    {
        QIcon icon = QIcon::fromTheme(icon_name);

        // if the icon could be resolved
        if (!icon.isNull()) {
            m_icon = icon;
        }
    }

    void xdg_toplevel_icon_v1_add_buffer(Resource *resource, struct ::wl_resource *icon, int32_t scale) override
    {
        if (!m_icon.name().isEmpty()) {
            // we already found a themed icon, we can ignore the buffer
            return;
        }

        GraphicsBuffer *buffer = m_display->bufferForResource(icon);
        Q_ASSERT(buffer);

        GraphicsBufferView view(buffer);
        if (view.isNull()) {
            wl_resource_post_error(resource->handle, XdgToplevelIconV1Interface::error_invalid_buffer, "Icon buffer invalid");
            return;
        }

        if (view.image()->width() != view.image()->height()) {
            wl_resource_post_error(resource->handle, XdgToplevelIconV1Interface::error_invalid_buffer, "Icon buffer is not a square");
            return;
        }

        QImage image = view.image()->copy();
        if (image.isNull()) {
            wl_resource_post_error(resource->handle, XdgToplevelIconV1Interface::error_invalid_buffer, "TopLevel icon invalid");
            return;
        }
        image.setDevicePixelRatio(scale);
        m_icon.addPixmap(QPixmap::fromImage(image));
    }

private:
    Display *m_display;
};

class XdgToplevelIconManagerV1InterfacePrivate : public QtWaylandServer::xdg_toplevel_icon_manager_v1
{
public:
    XdgToplevelIconManagerV1InterfacePrivate(XdgToplevelIconManagerV1Interface *q, Display *display)
        : xdg_toplevel_icon_manager_v1(*display, version)
        , m_display(display)
    {
    }

protected:
    void xdg_toplevel_icon_manager_v1_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void xdg_toplevel_icon_manager_v1_bind_resource(Resource *resource) override
    {
        const int preferredSize = 96;
        // We have no meaningful sizes to send here.
        // We don't know what size plasma or any plugins will render at
        // pick something big
        send_icon_size(resource->handle, preferredSize);
        send_done(resource->handle);
    }

    virtual void xdg_toplevel_icon_manager_v1_create_icon(Resource *resource, uint32_t id) override
    {
        wl_resource *iconResource = wl_resource_create(resource->client(), &xdg_toplevel_icon_v1_interface, resource->version(), id);
        if (!iconResource) {
            wl_resource_post_no_memory(resource->handle);
            return;
        }

        new XdgToplevelIconV1Interface(iconResource, m_display);
    }

    virtual void xdg_toplevel_icon_manager_v1_set_icon(Resource *resource, struct ::wl_resource *toplevel, struct ::wl_resource *icon) override
    {
        XdgToplevelInterfacePrivate *toplevelPrivate = XdgToplevelInterfacePrivate::get(toplevel);
        Q_ASSERT(toplevelPrivate);
        if (!icon) {
            toplevelPrivate->customIcon = QIcon();
            Q_EMIT toplevelPrivate->q->customIconChanged();
            return;
        }

        XdgToplevelIconV1Interface *iconIface = XdgToplevelIconV1Interface::get(icon);
        Q_ASSERT(iconIface);

        toplevelPrivate->customIcon = iconIface->m_icon;
        Q_EMIT toplevelPrivate->q->customIconChanged();
    }

private:
    Display *m_display;
};

XdgToplevelIconManagerV1Interface::XdgToplevelIconManagerV1Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<XdgToplevelIconManagerV1InterfacePrivate>(this, display))
{
}

XdgToplevelIconManagerV1Interface::~XdgToplevelIconManagerV1Interface() = default;
}

#include  "wayland/moc_xdgtoplevelicon_v1.cpp"
