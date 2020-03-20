/********************************************************************
Copyright 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

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
#include "tablet_interface.h"
#include "display.h"
#include "resource_p.h"
#include "seat_interface.h"
#include "surface_interface.h"

#include "qwayland-server-tablet-unstable-v2.h"
#include <QHash>

using namespace KWayland;
using namespace Server;

static int s_version = 1;

class TabletInterface::Private : public QtWaylandServer::zwp_tablet_v2
{
public:
    Private(TabletInterface* q, uint32_t vendorId, uint32_t productId, const QString name, const QStringList &paths)
        : zwp_tablet_v2()
        , q(q)
        , m_vendorId(vendorId)
        , m_productId(productId)
        , m_name(name)
        , m_paths(paths)
    {
    }

    wl_resource *resourceForSurface(SurfaceInterface *surface) const
    {
        ClientConnection *client = surface->client();
        QtWaylandServer::zwp_tablet_v2::Resource *r = resourceMap().value(*client);
        return r ? r->handle : nullptr;
    }

    void zwp_tablet_v2_destroy_resource(QtWaylandServer::zwp_tablet_v2::Resource * resource) override
    {
        Q_UNUSED(resource);
        if (removed && resourceMap().isEmpty()) {
            delete q;
        }
    }

    TabletInterface *const q;
    const uint32_t m_vendorId;
    const uint32_t m_productId;
    const QString m_name;
    const QStringList m_paths;
    bool removed = false;
};

TabletInterface::TabletInterface(uint32_t vendorId, uint32_t productId,
                                 const QString &name, const QStringList &paths,
                                 QObject *parent)
    : QObject(parent)
    , d(new Private(this, vendorId, productId, name, paths))
{
}

TabletInterface::~TabletInterface() = default;

bool TabletInterface::isSurfaceSupported(SurfaceInterface *surface) const
{
    return d->resourceForSurface(surface);
}

void TabletInterface::sendRemoved()
{
    d->removed = true;
    for (QtWaylandServer::zwp_tablet_v2::Resource *resource : d->resourceMap()) {
        d->send_removed(resource->handle);
    }
}

class TabletCursor::Private
{
public:
    Private(TabletCursor* q) : q(q) {}

    void update(quint32 serial, SurfaceInterface *surface, const QPoint &hotspot)
    {
        const bool diff = m_serial != serial && m_surface != surface && m_hotspot != hotspot;
        m_serial = serial;
        m_surface = surface;
        m_hotspot = hotspot;
        if (diff)
            Q_EMIT q->changed();
    }

    TabletCursor* const q;

    quint32 m_serial = 0;
    SurfaceInterface* m_surface = nullptr;
    QPoint m_hotspot;
};

TabletCursor::TabletCursor()
    : QObject()
    , d(new Private(this))
{
}

TabletCursor::~TabletCursor() = default;

QPoint TabletCursor::hotspot() const
{
    return d->m_hotspot;
}

quint32 TabletCursor::enteredSerial() const
{
    return d->m_serial;
}

SurfaceInterface* TabletCursor::surface() const
{
    return d->m_surface;
}

class TabletToolInterface::Private : public QtWaylandServer::zwp_tablet_tool_v2
{
public:
    Private(TabletToolInterface* q, Display *display, Type type, uint32_t hsh, uint32_t hsl, uint32_t hih,
            uint32_t hil, const QVector<Capability>& capabilities)
        : zwp_tablet_tool_v2()
        , m_display(display)
        , m_type(type)
        , m_hardwareSerialHigh(hsh)
        , m_hardwareSerialLow(hsl)
        , m_hardwareIdHigh(hih)
        , m_hardwareIdLow(hil)
        , m_capabilities(capabilities)
        , q(q)
    {
    }


    wl_resource *targetResource()
    {
        if (!m_surface)
            return nullptr;

        ClientConnection *client = m_surface->client();
        const Resource *r = resourceMap().value(*client);
        return r ? r->handle : nullptr;
    }

    quint64 hardwareId() const
    {
        return quint64(quint64(m_hardwareIdHigh) << 32) + m_hardwareIdLow;
    }
    quint64 hardwareSerial() const
    {
        return quint64(quint64(m_hardwareSerialHigh) << 32) + m_hardwareSerialLow;
    }

    void zwp_tablet_tool_v2_bind_resource(QtWaylandServer::zwp_tablet_tool_v2::Resource * resource) override
    {
        TabletCursor *&c = m_cursors[resource->handle];
        if (!c)
            c = new TabletCursor;
    }

    void zwp_tablet_tool_v2_set_cursor(Resource * resource, uint32_t serial, struct ::wl_resource * _surface, int32_t hotspot_x, int32_t hotspot_y) override
    {
        TabletCursor *c = m_cursors[resource->handle];
        c->d->update(serial, SurfaceInterface::get(_surface), {hotspot_x, hotspot_y});
        if (resource->handle == targetResource())
            q->cursorChanged(c);
    }

    void zwp_tablet_tool_v2_destroy_resource(Resource * resource) override {
        delete m_cursors.take(resource->handle);
    }

    Display *const m_display;
    bool m_cleanup = false;
    QPointer<SurfaceInterface> m_surface;
    QPointer<TabletInterface> m_lastTablet;
    const uint32_t m_type;
    const uint32_t m_hardwareSerialHigh, m_hardwareSerialLow;
    const uint32_t m_hardwareIdHigh, m_hardwareIdLow;
    const QVector<Capability> m_capabilities;
    QHash<wl_resource *, TabletCursor *> m_cursors;
    TabletToolInterface *const q;
};

TabletToolInterface::TabletToolInterface(Display *display, Type type, uint32_t hsh,
                                         uint32_t hsl, uint32_t hih, uint32_t hil,
                                         const QVector<Capability>& capabilities,
                                         QObject *parent)
    : QObject(parent)
    , d(new Private(this, display, type, hsh, hsl, hih, hil, capabilities))
{
}

TabletToolInterface::~TabletToolInterface() = default;

void TabletToolInterface::setCurrentSurface(SurfaceInterface *surface)
{
    if (d->m_surface == surface)
        return;

    TabletInterface *const lastTablet = d->m_lastTablet;
    if (d->m_surface && d->resourceMap().contains(*d->m_surface->client())) {
        sendProximityOut();
        sendFrame(0);
    }

    d->m_surface = surface;

    if (lastTablet && lastTablet->d->resourceForSurface(surface)) {
        sendProximityIn(lastTablet);
    } else {
        d->m_lastTablet = lastTablet;
    }

    Q_EMIT cursorChanged(d->m_cursors.value(d->targetResource()));
}

bool TabletToolInterface::isClientSupported() const
{
    return d->m_surface && d->targetResource();
}

void TabletToolInterface::sendButton(uint32_t button, bool pressed)
{
    d->send_button(d->targetResource(), d->m_display->nextSerial(), button,
                   pressed ? QtWaylandServer::zwp_tablet_tool_v2::button_state_pressed
                           : QtWaylandServer::zwp_tablet_tool_v2::button_state_released);
}

void TabletToolInterface::sendMotion(const QPointF &pos)
{
    d->send_motion(d->targetResource(), wl_fixed_from_double(pos.x()),
                                        wl_fixed_from_double(pos.y()));
}

void TabletToolInterface::sendDistance(uint32_t distance)
{
    d->send_distance(d->targetResource(), distance);
}

void TabletToolInterface::sendFrame(uint32_t time)
{
    d->send_frame(d->targetResource(), time);

    if (d->m_cleanup) {
        d->m_surface = nullptr;
        d->m_lastTablet = nullptr;
        d->m_cleanup = false;
    }
}

void TabletToolInterface::sendPressure(uint32_t pressure)
{
    d->send_pressure(d->targetResource(), pressure);
}

void TabletToolInterface::sendRotation(qreal rotation)
{
    d->send_rotation(d->targetResource(), wl_fixed_from_double(rotation));
}

void TabletToolInterface::sendSlider(int32_t position)
{
    d->send_slider(d->targetResource(), position);
}

void TabletToolInterface::sendTilt(qreal degreesX, qreal degreesY)
{
    d->send_tilt(d->targetResource(), wl_fixed_from_double(degreesX),
                                      wl_fixed_from_double(degreesY));
}

void TabletToolInterface::sendWheel(int32_t degrees, int32_t clicks)
{
    d->send_wheel(d->targetResource(), degrees, clicks);
}

void TabletToolInterface::sendProximityIn(TabletInterface *tablet)
{
    wl_resource* tabletResource = tablet->d->resourceForSurface(d->m_surface);
    d->send_proximity_in(d->targetResource(), d->m_display->nextSerial(),
                         tabletResource, d->m_surface->resource());
    d->m_lastTablet = tablet;
}

void TabletToolInterface::sendProximityOut()
{
    d->send_proximity_out(d->targetResource());
    d->m_cleanup = true;
}

void TabletToolInterface::sendDown()
{
    d->send_down(d->targetResource(), d->m_display->nextSerial());
}

void TabletToolInterface::sendUp()
{
    d->send_up(d->targetResource());
}

void TabletToolInterface::sendRemoved()
{
    for (QtWaylandServer::zwp_tablet_tool_v2::Resource *resource : d->resourceMap()) {
        d->send_removed(resource->handle);
    }
}

class TabletSeatInterface::Private : public QtWaylandServer::zwp_tablet_seat_v2
{
public:
    Private(Display *display, TabletSeatInterface *q)
        : zwp_tablet_seat_v2()
        , q(q)
        , m_display(display)
    {
    }

    void zwp_tablet_seat_v2_bind_resource(Resource *resource) override
    {
        for (auto iface : qAsConst(m_tablets)) {
            sendTabletAdded(resource, iface);
        }

        for (auto *tool : qAsConst(m_tools)) {
            sendToolAdded(resource, tool);
        }
    }

    void sendToolAdded(Resource *resource, TabletToolInterface *tool)
    {
        wl_resource *toolResource = tool->d->add(resource->client(), resource->version())->handle;
        send_tool_added(resource->handle, toolResource);

        tool->d->send_type(toolResource, tool->d->m_type);
        tool->d->send_hardware_serial(toolResource, tool->d->m_hardwareSerialHigh,
                                                    tool->d->m_hardwareSerialLow);
        tool->d->send_hardware_id_wacom(toolResource, tool->d->m_hardwareIdHigh,
                                                      tool->d->m_hardwareIdLow);
        for (uint32_t cap : qAsConst(tool->d->m_capabilities)) {
            tool->d->send_capability(toolResource, cap);
        }
        tool->d->send_done(toolResource);
    }
    void sendTabletAdded(Resource *resource, TabletInterface *tablet)
    {
        wl_resource *tabletResource = tablet->d->add(resource->client(), resource->version())->handle;
        send_tablet_added(resource->handle, tabletResource);

        tablet->d->send_name(tabletResource, tablet->d->m_name);
        if (tablet->d->m_vendorId && tablet->d->m_productId) {
            tablet->d->send_id(tabletResource, tablet->d->m_vendorId, tablet->d->m_productId);
        }
        for (const QString &path : qAsConst(tablet->d->m_paths)) {
            tablet->d->send_path(tabletResource, path);
        }
        tablet->d->send_done(tabletResource);
    }

    TabletSeatInterface *const q;
    QVector<TabletToolInterface *> m_tools;
    QHash<QString, TabletInterface *> m_tablets;
    Display *const m_display;
};

TabletSeatInterface::TabletSeatInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new Private(display, this))
{
}

TabletSeatInterface::~TabletSeatInterface() = default;

TabletToolInterface *TabletSeatInterface::addTool(TabletToolInterface::Type type,
                                                  quint64 hardwareSerial,
                                                  quint64 hardwareId,
                                                  const QVector<TabletToolInterface::Capability> &capabilities)
{
    constexpr auto MAX_UINT_32 = std::numeric_limits<quint32>::max();
    auto tool = new TabletToolInterface(d->m_display,
                                        type, hardwareSerial >> 32, hardwareSerial & MAX_UINT_32,
                                               hardwareId >> 32, hardwareId & MAX_UINT_32, capabilities, this);
    for (QtWaylandServer::zwp_tablet_seat_v2::Resource *resource : d->resourceMap()) {
        d->sendToolAdded(resource, tool);
    }

    d->m_tools.append(tool);
    QObject::connect(tool, &QObject::destroyed, this, [this](QObject *object) {
        auto tti = static_cast<TabletToolInterface *>(object);
        tti->d->send_removed();
        d->m_tools.removeAll(tti);
    });
    return tool;
}

TabletInterface *TabletSeatInterface::addTablet(uint32_t vendorId, uint32_t productId,
                                                const QString &sysname,
                                                const QString &name,
                                                const QStringList &paths)
{
    auto iface = new TabletInterface(vendorId, productId, name, paths, this);

    for (QtWaylandServer::zwp_tablet_seat_v2::Resource *r : d->resourceMap()) {
        d->sendTabletAdded(r, iface);
    }

    d->m_tablets[sysname] = iface;
    return iface;
}

void TabletSeatInterface::removeTablet(const QString &sysname)
{
    auto tablet = d->m_tablets.take(sysname);
    if (tablet) {
        tablet->sendRemoved();
    }
}

TabletToolInterface *TabletSeatInterface::toolByHardwareId(quint64 hardwareId) const
{
    for (TabletToolInterface *tool : d->m_tools) {
        if (tool->d->hardwareId() == hardwareId)
            return tool;
    }
    return nullptr;
}

TabletToolInterface *TabletSeatInterface::toolByHardwareSerial(quint64 hardwareSerial) const
{
    for (TabletToolInterface *tool : d->m_tools) {
        if (tool->d->hardwareSerial() == hardwareSerial)
            return tool;
    }
    return nullptr;
}

TabletInterface *TabletSeatInterface::tabletByName(const QString &name) const
{
    return d->m_tablets.value(name);
}

class TabletManagerInterface::Private : public QtWaylandServer::zwp_tablet_manager_v2
{
public:
    Private(Display *display, TabletManagerInterface *q)
        : zwp_tablet_manager_v2(*display, s_version)
        , q(q)
        , m_display(display)
    {
    }

    void zwp_tablet_manager_v2_get_tablet_seat(Resource *resource, uint32_t tablet_seat,
                                               struct ::wl_resource *seat_resource) override {
        SeatInterface* seat = SeatInterface::get(seat_resource);
        TabletSeatInterface *tsi = get(seat);
        tsi->d->add(resource->client(), tablet_seat, s_version);
    }

    TabletSeatInterface *get(SeatInterface *seat)
    {
        TabletSeatInterface *&tabletSeat = m_seats[seat];
        if (!tabletSeat) {
            tabletSeat = new TabletSeatInterface(m_display, q);
        }
        return tabletSeat;
    }

    TabletManagerInterface *const q;
    Display *const m_display;
    QHash<SeatInterface *, TabletSeatInterface *> m_seats;
};

TabletManagerInterface::TabletManagerInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new Private(display, this))
{
}

TabletSeatInterface *TabletManagerInterface::seat(SeatInterface *seat) const
{
    return d->get(seat);
}

TabletManagerInterface::~TabletManagerInterface() = default;
