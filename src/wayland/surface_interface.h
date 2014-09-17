/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_WAYLAND_SERVER_SURFACE_INTERFACE_H
#define KWIN_WAYLAND_SERVER_SURFACE_INTERFACE_H

#include "output_interface.h"

#include <QObject>
#include <QRegion>

#include <wayland-server.h>

#include <kwaylandserver_export.h>

namespace KWin
{
namespace WaylandServer
{
class BufferInterface;
class CompositorInterface;

class KWAYLANDSERVER_EXPORT SurfaceInterface : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QRegion damage READ damage NOTIFY damaged)
    Q_PROPERTY(QRegion opaque READ opaque NOTIFY opaqueChanged)
    Q_PROPERTY(QRegion input READ input NOTIFY inputChanged)
    Q_PROPERTY(qint32 scale READ scale NOTIFY scaleChanged)
    Q_PROPERTY(KWin::WaylandServer::OutputInterface::Transform transform READ transform NOTIFY transformChanged)
public:
    virtual ~SurfaceInterface();

    void create(wl_client *client, quint32 version, quint32 id);

    void frameRendered(quint32 msec);

    wl_resource *surface() const {
        return m_surface;
    }
    wl_client *client() const {
        return m_client;
    }

    QRegion damage() const {
        return m_current.damage;
    }
    QRegion opaque() const {
        return m_current.opaque;
    }
    QRegion input() const {
        return m_current.input;
    }
    qint32 scale() const {
        return m_current.scale;
    }
    OutputInterface::Transform transform() const {
        return m_current.transform;
    }
    BufferInterface *buffer() {
        return m_current.buffer;
    }
    QPoint offset() const {
        return m_current.offset;
    }

Q_SIGNALS:
    void damaged(const QRegion&);
    void opaqueChanged(const QRegion&);
    void inputChanged(const QRegion&);
    void scaleChanged(qint32);
    void transformChanged(KWin::WaylandServer::OutputInterface::Transform);

private:
    struct State {
        QRegion damage = QRegion();
        QRegion opaque = QRegion();
        QRegion input = QRegion();
        qint32 scale = 1;
        OutputInterface::Transform transform = OutputInterface::Transform::Normal;
        QList<wl_resource*> callbacks = QList<wl_resource*>();
        QPoint offset = QPoint();
        BufferInterface *buffer = nullptr;
    };
    friend class CompositorInterface;
    explicit SurfaceInterface(CompositorInterface *parent);
    static void unbind(wl_resource *r);
    static void destroyFrameCallback(wl_resource *r);

    static const struct wl_surface_interface s_interface;
    static void destroyCallback(wl_client *client, wl_resource *resource);
    static void attachCallback(wl_client *client, wl_resource *resource, wl_resource *buffer, int32_t sx, int32_t sy);
    static void damageCallback(wl_client *client, wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height);
    static void frameCallaback(wl_client *client, wl_resource *resource, uint32_t callback);
    static void opaqueRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region);
    static void inputRegionCallback(wl_client *client, wl_resource *resource, wl_resource *region);
    static void commitCallback(wl_client *client, wl_resource *resource);
    // since version 2
    static void bufferTransformCallback(wl_client *client, wl_resource *resource, int32_t transform);
    // since version 3
    static void bufferScaleCallback(wl_client *client, wl_resource *resource, int32_t scale);

    static SurfaceInterface *cast(wl_resource *r) {
        return reinterpret_cast<SurfaceInterface*>(wl_resource_get_user_data(r));
    }
    void destroy();
    void commit();
    void damage(const QRect &rect);
    void setScale(qint32 scale);
    void setTransform(OutputInterface::Transform transform);
    void addFrameCallback(uint32_t callback);
    void attachBuffer(wl_resource *buffer, const QPoint &offset);
    CompositorInterface *m_compositor;
    wl_resource *m_surface;
    wl_client *m_client;
    State m_current;
    State m_pending;
};

}
}

#endif
