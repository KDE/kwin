/*
    SPDX-FileCopyrightText: 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "tablet_v2_interface.h"
#include "display.h"
#include "seat_interface.h"
#include "surface_interface.h"

#include "qwayland-server-tablet-unstable-v2.h"
#include <QHash>

using namespace KWaylandServer;

static int s_version = 1;

class TabletV2Interface::Private : public QtWaylandServer::zwp_tablet_v2
{
public:
    Private(TabletV2Interface *q, uint32_t vendorId, uint32_t productId, const QString name, const QStringList &paths)
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

    TabletV2Interface *const q;
    const uint32_t m_vendorId;
    const uint32_t m_productId;
    const QString m_name;
    const QStringList m_paths;
    bool removed = false;
};

TabletV2Interface::TabletV2Interface(uint32_t vendorId, uint32_t productId,
                                     const QString &name, const QStringList &paths,
                                     QObject *parent)
    : QObject(parent)
    , d(new Private(this, vendorId, productId, name, paths))
{
}

TabletV2Interface::~TabletV2Interface() = default;

bool TabletV2Interface::isSurfaceSupported(SurfaceInterface *surface) const
{
    return d->resourceForSurface(surface);
}

void TabletV2Interface::sendRemoved()
{
    d->removed = true;
    for (QtWaylandServer::zwp_tablet_v2::Resource *resource : d->resourceMap()) {
        d->send_removed(resource->handle);
    }
}

class TabletCursorV2::Private
{
public:
    Private(TabletCursorV2 *q) : q(q) {}

    void update(quint32 serial, SurfaceInterface *surface, const QPoint &hotspot)
    {
        const bool diff = m_serial != serial && m_surface != surface && m_hotspot != hotspot;
        m_serial = serial;
        m_surface = surface;
        m_hotspot = hotspot;
        if (diff)
            Q_EMIT q->changed();
    }

    TabletCursorV2 *const q;

    quint32 m_serial = 0;
    SurfaceInterface* m_surface = nullptr;
    QPoint m_hotspot;
};

TabletCursorV2::TabletCursorV2()
    : QObject()
    , d(new Private(this))
{
}

TabletCursorV2::~TabletCursorV2() = default;

QPoint TabletCursorV2::hotspot() const
{
    return d->m_hotspot;
}

quint32 TabletCursorV2::enteredSerial() const
{
    return d->m_serial;
}

SurfaceInterface *TabletCursorV2::surface() const
{
    return d->m_surface;
}

class TabletToolV2Interface::Private : public QtWaylandServer::zwp_tablet_tool_v2
{
public:
    Private(TabletToolV2Interface *q, Display *display, Type type, uint32_t hsh, uint32_t hsl, uint32_t hih,
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
        TabletCursorV2 *&c = m_cursors[resource->handle];
        if (!c)
            c = new TabletCursorV2;
    }

    void zwp_tablet_tool_v2_set_cursor(Resource * resource, uint32_t serial, struct ::wl_resource * _surface, int32_t hotspot_x, int32_t hotspot_y) override
    {
        TabletCursorV2 *c = m_cursors[resource->handle];
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
    QPointer<TabletV2Interface> m_lastTablet;
    const uint32_t m_type;
    const uint32_t m_hardwareSerialHigh, m_hardwareSerialLow;
    const uint32_t m_hardwareIdHigh, m_hardwareIdLow;
    const QVector<Capability> m_capabilities;
    QHash<wl_resource *, TabletCursorV2 *> m_cursors;
    TabletToolV2Interface *const q;
};

TabletToolV2Interface::TabletToolV2Interface(Display *display, Type type, uint32_t hsh,
                                             uint32_t hsl, uint32_t hih, uint32_t hil,
                                             const QVector<Capability>& capabilities,
                                             QObject *parent)
    : QObject(parent)
    , d(new Private(this, display, type, hsh, hsl, hih, hil, capabilities))
{
}

TabletToolV2Interface::~TabletToolV2Interface() = default;

void TabletToolV2Interface::setCurrentSurface(SurfaceInterface *surface)
{
    if (d->m_surface == surface)
        return;

    TabletV2Interface *const lastTablet = d->m_lastTablet;
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

bool TabletToolV2Interface::isClientSupported() const
{
    return d->m_surface && d->targetResource();
}

void TabletToolV2Interface::sendButton(uint32_t button, bool pressed)
{
    d->send_button(d->targetResource(), d->m_display->nextSerial(), button,
                   pressed ? QtWaylandServer::zwp_tablet_tool_v2::button_state_pressed
                           : QtWaylandServer::zwp_tablet_tool_v2::button_state_released);
}

void TabletToolV2Interface::sendMotion(const QPointF &pos)
{
    d->send_motion(d->targetResource(), wl_fixed_from_double(pos.x()),
                                        wl_fixed_from_double(pos.y()));
}

void TabletToolV2Interface::sendDistance(uint32_t distance)
{
    d->send_distance(d->targetResource(), distance);
}

void TabletToolV2Interface::sendFrame(uint32_t time)
{
    d->send_frame(d->targetResource(), time);

    if (d->m_cleanup) {
        d->m_surface = nullptr;
        d->m_lastTablet = nullptr;
        d->m_cleanup = false;
    }
}

void TabletToolV2Interface::sendPressure(uint32_t pressure)
{
    d->send_pressure(d->targetResource(), pressure);
}

void TabletToolV2Interface::sendRotation(qreal rotation)
{
    d->send_rotation(d->targetResource(), wl_fixed_from_double(rotation));
}

void TabletToolV2Interface::sendSlider(int32_t position)
{
    d->send_slider(d->targetResource(), position);
}

void TabletToolV2Interface::sendTilt(qreal degreesX, qreal degreesY)
{
    d->send_tilt(d->targetResource(), wl_fixed_from_double(degreesX),
                                      wl_fixed_from_double(degreesY));
}

void TabletToolV2Interface::sendWheel(int32_t degrees, int32_t clicks)
{
    d->send_wheel(d->targetResource(), degrees, clicks);
}

void TabletToolV2Interface::sendProximityIn(TabletV2Interface *tablet)
{
    wl_resource* tabletResource = tablet->d->resourceForSurface(d->m_surface);
    d->send_proximity_in(d->targetResource(), d->m_display->nextSerial(),
                         tabletResource, d->m_surface->resource());
    d->m_lastTablet = tablet;
}

void TabletToolV2Interface::sendProximityOut()
{
    d->send_proximity_out(d->targetResource());
    d->m_cleanup = true;
}

void TabletToolV2Interface::sendDown()
{
    d->send_down(d->targetResource(), d->m_display->nextSerial());
}

void TabletToolV2Interface::sendUp()
{
    d->send_up(d->targetResource());
}

void TabletToolV2Interface::sendRemoved()
{
    for (QtWaylandServer::zwp_tablet_tool_v2::Resource *resource : d->resourceMap()) {
        d->send_removed(resource->handle);
    }
}

class TabletSeatV2Interface::Private : public QtWaylandServer::zwp_tablet_seat_v2
{
public:
    Private(Display *display, TabletSeatV2Interface *q)
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

    void sendToolAdded(Resource *resource, TabletToolV2Interface *tool)
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
    void sendTabletAdded(Resource *resource, TabletV2Interface *tablet)
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

    TabletSeatV2Interface *const q;
    QVector<TabletToolV2Interface *> m_tools;
    QHash<QString, TabletV2Interface *> m_tablets;
    Display *const m_display;
};

TabletSeatV2Interface::TabletSeatV2Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new Private(display, this))
{
}

TabletSeatV2Interface::~TabletSeatV2Interface() = default;

TabletToolV2Interface *TabletSeatV2Interface::addTool(TabletToolV2Interface::Type type,
                                                      quint64 hardwareSerial,
                                                      quint64 hardwareId,
                                                      const QVector<TabletToolV2Interface::Capability> &capabilities)
{
    constexpr auto MAX_UINT_32 = std::numeric_limits<quint32>::max();
    auto tool = new TabletToolV2Interface(d->m_display,
                                          type, hardwareSerial >> 32, hardwareSerial & MAX_UINT_32,
                                                 hardwareId >> 32, hardwareId & MAX_UINT_32, capabilities, this);
    for (QtWaylandServer::zwp_tablet_seat_v2::Resource *resource : d->resourceMap()) {
        d->sendToolAdded(resource, tool);
    }

    d->m_tools.append(tool);
    QObject::connect(tool, &QObject::destroyed, this, [this](QObject *object) {
        auto tti = static_cast<TabletToolV2Interface *>(object);
        tti->d->send_removed();
        d->m_tools.removeAll(tti);
    });
    return tool;
}

TabletV2Interface *TabletSeatV2Interface::addTablet(uint32_t vendorId, uint32_t productId,
                                                    const QString &sysname,
                                                    const QString &name,
                                                    const QStringList &paths)
{
    auto iface = new TabletV2Interface(vendorId, productId, name, paths, this);

    for (QtWaylandServer::zwp_tablet_seat_v2::Resource *r : d->resourceMap()) {
        d->sendTabletAdded(r, iface);
    }

    d->m_tablets[sysname] = iface;
    return iface;
}

void TabletSeatV2Interface::removeTablet(const QString &sysname)
{
    auto tablet = d->m_tablets.take(sysname);
    if (tablet) {
        tablet->sendRemoved();
    }
}

TabletToolV2Interface *TabletSeatV2Interface::toolByHardwareId(quint64 hardwareId) const
{
    for (TabletToolV2Interface *tool : d->m_tools) {
        if (tool->d->hardwareId() == hardwareId)
            return tool;
    }
    return nullptr;
}

TabletToolV2Interface *TabletSeatV2Interface::toolByHardwareSerial(quint64 hardwareSerial) const
{
    for (TabletToolV2Interface *tool : d->m_tools) {
        if (tool->d->hardwareSerial() == hardwareSerial)
            return tool;
    }
    return nullptr;
}

TabletV2Interface *TabletSeatV2Interface::tabletByName(const QString &name) const
{
    return d->m_tablets.value(name);
}

class TabletManagerV2Interface::Private : public QtWaylandServer::zwp_tablet_manager_v2
{
public:
    Private(Display *display, TabletManagerV2Interface *q)
        : zwp_tablet_manager_v2(*display, s_version)
        , q(q)
        , m_display(display)
    {
    }

    void zwp_tablet_manager_v2_get_tablet_seat(Resource *resource, uint32_t tablet_seat,
                                               struct ::wl_resource *seat_resource) override {
        SeatInterface* seat = SeatInterface::get(seat_resource);
        TabletSeatV2Interface *tsi = get(seat);
        tsi->d->add(resource->client(), tablet_seat, s_version);
    }

    TabletSeatV2Interface *get(SeatInterface *seat)
    {
        TabletSeatV2Interface *&tabletSeat = m_seats[seat];
        if (!tabletSeat) {
            tabletSeat = new TabletSeatV2Interface(m_display, q);
        }
        return tabletSeat;
    }

    TabletManagerV2Interface *const q;
    Display *const m_display;
    QHash<SeatInterface *, TabletSeatV2Interface *> m_seats;
};

TabletManagerV2Interface::TabletManagerV2Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new Private(display, this))
{
}

TabletSeatV2Interface *TabletManagerV2Interface::seat(SeatInterface *seat) const
{
    return d->get(seat);
}

TabletManagerV2Interface::~TabletManagerV2Interface() = default;
