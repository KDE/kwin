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

namespace KWaylandServer
{

static int s_version = 1;

class TabletV2InterfacePrivate : public QtWaylandServer::zwp_tablet_v2
{
public:
    TabletV2InterfacePrivate(TabletV2Interface *q, uint32_t vendorId, uint32_t productId, const QString &name, const QStringList &paths)
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
        Resource *r = resourceMap().value(*client);
        return r ? r->handle : nullptr;
    }

    void zwp_tablet_v2_destroy_resource(QtWaylandServer::zwp_tablet_v2::Resource * resource) override
    {
        Q_UNUSED(resource);
        if (m_removed && resourceMap().isEmpty()) {
            delete q;
        }
    }

    TabletV2Interface *const q;
    TabletPadV2Interface *m_pad = nullptr;
    const uint32_t m_vendorId;
    const uint32_t m_productId;
    const QString m_name;
    const QStringList m_paths;
    bool m_removed = false;
};

TabletV2Interface::TabletV2Interface(uint32_t vendorId, uint32_t productId,
                                     const QString &name, const QStringList &paths,
                                     QObject *parent)
    : QObject(parent)
    , d(new TabletV2InterfacePrivate(this, vendorId, productId, name, paths))
{
}

TabletV2Interface::~TabletV2Interface() = default;

bool TabletV2Interface::isSurfaceSupported(SurfaceInterface *surface) const
{
    return d->resourceForSurface(surface);
}

void TabletV2Interface::sendRemoved()
{
    d->m_removed = true;
    for (QtWaylandServer::zwp_tablet_v2::Resource *resource : d->resourceMap()) {
        d->send_removed(resource->handle);
    }
}

TabletPadV2Interface *TabletV2Interface::pad() const
{
    return d->m_pad;
}

class TabletCursorV2Private
{
public:
    TabletCursorV2Private(TabletCursorV2 *q) : q(q) {}

    void update(quint32 serial, SurfaceInterface *surface, const QPoint &hotspot)
    {
        const bool diff = m_serial != serial || m_surface != surface || m_hotspot != hotspot;
        if (diff) {
            m_serial = serial;
            m_surface = surface;
            m_hotspot = hotspot;

            Q_EMIT q->changed();
        }
    }

    TabletCursorV2 *const q;

    quint32 m_serial = 0;
    SurfaceInterface* m_surface = nullptr;
    QPoint m_hotspot;
};

TabletCursorV2::TabletCursorV2()
    : QObject()
    , d(new TabletCursorV2Private(this))
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

class TabletToolV2InterfacePrivate : public QtWaylandServer::zwp_tablet_tool_v2
{
public:
    TabletToolV2InterfacePrivate(TabletToolV2Interface *q, Display *display,
                                 TabletToolV2Interface::Type type, uint32_t hsh, uint32_t hsl,
                                 uint32_t hih, uint32_t hil,
                                 const QVector<TabletToolV2Interface::Capability>& capabilities)
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
        if (m_removed && resourceMap().isEmpty()) {
            delete q;
        }
    }

    void zwp_tablet_tool_v2_destroy(Resource *resource) override {
        wl_resource_destroy(resource->handle);
    }

    Display *const m_display;
    bool m_cleanup = false;
    bool m_removed = false;
    QPointer<SurfaceInterface> m_surface;
    QPointer<TabletV2Interface> m_lastTablet;
    const uint32_t m_type;
    const uint32_t m_hardwareSerialHigh, m_hardwareSerialLow;
    const uint32_t m_hardwareIdHigh, m_hardwareIdLow;
    const QVector<TabletToolV2Interface::Capability> m_capabilities;
    QHash<wl_resource *, TabletCursorV2 *> m_cursors;
    TabletToolV2Interface *const q;
};

TabletToolV2Interface::TabletToolV2Interface(Display *display, Type type, uint32_t hsh,
                                             uint32_t hsl, uint32_t hih, uint32_t hil,
                                             const QVector<Capability>& capabilities,
                                             QObject *parent)
    : QObject(parent)
    , d(new TabletToolV2InterfacePrivate(this, display, type, hsh, hsl, hih, hil, capabilities))
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
    d->m_removed = true;

    for (QtWaylandServer::zwp_tablet_tool_v2::Resource *resource : d->resourceMap()) {
        d->send_removed(resource->handle);
    }
}

class TabletPadRingV2InterfacePrivate : public QtWaylandServer::zwp_tablet_pad_ring_v2
{
public:
    TabletPadRingV2InterfacePrivate(TabletPadRingV2Interface *q)
        : zwp_tablet_pad_ring_v2()
        , q(q)
    {
    }

    wl_resource *resourceForSurface(SurfaceInterface *surface) const
    {
        ClientConnection *client = surface->client();
        Resource *r = resourceMap().value(*client);
        return r ? r->handle : nullptr;
    }

    void zwp_tablet_pad_ring_v2_destroy_resource(Resource *resource) override {
        Q_UNUSED(resource)
        if (m_pad->isRemoved() && resourceMap().isEmpty()) {
            delete q;
        }
    }

    void zwp_tablet_pad_ring_v2_destroy(Resource *resource) override {
        wl_resource_destroy(resource->handle);
    }
    TabletPadRingV2Interface *const q;
    TabletPadV2Interface *m_pad;
};

TabletPadRingV2Interface::TabletPadRingV2Interface(TabletPadV2Interface *parent)
    : QObject(parent)
    , d(new TabletPadRingV2InterfacePrivate(this))
{
    d->m_pad = parent;
}

TabletPadRingV2Interface::~TabletPadRingV2Interface() = default;

void TabletPadRingV2Interface::sendAngle(qreal angle)
{
    d->send_angle(d->resourceForSurface(d->m_pad->currentSurface()), wl_fixed_from_double(angle));
}

void TabletPadRingV2Interface::sendFrame(quint32 time)
{
    d->send_frame(d->resourceForSurface(d->m_pad->currentSurface()), time);
}

void TabletPadRingV2Interface::sendSource(Source source)
{
    d->send_source(d->resourceForSurface(d->m_pad->currentSurface()), source);
}

void TabletPadRingV2Interface::sendStop()
{
    d->send_stop(d->resourceForSurface(d->m_pad->currentSurface()));
}

class TabletPadStripV2InterfacePrivate : public QtWaylandServer::zwp_tablet_pad_strip_v2
{
public:
    TabletPadStripV2InterfacePrivate(TabletPadStripV2Interface *q)
        : zwp_tablet_pad_strip_v2()
        , q(q)
    {
    }

    wl_resource *resourceForSurface(SurfaceInterface *surface) const
    {
        ClientConnection *client = surface->client();
        Resource *r = resourceMap().value(*client);
        return r ? r->handle : nullptr;
    }

    void zwp_tablet_pad_strip_v2_destroy_resource(Resource *resource) override {
        Q_UNUSED(resource)
        if (m_pad->isRemoved() && resourceMap().isEmpty()) {
            delete q;
        }
    }

    void zwp_tablet_pad_strip_v2_destroy(Resource *resource) override {
        wl_resource_destroy(resource->handle);
    }
    TabletPadV2Interface *m_pad = nullptr;
    TabletPadStripV2Interface *const q;
};

TabletPadStripV2Interface::TabletPadStripV2Interface(TabletPadV2Interface *parent)
    : QObject(parent)
    , d(new TabletPadStripV2InterfacePrivate(this))
{
    d->m_pad = parent;
}

TabletPadStripV2Interface::~TabletPadStripV2Interface() = default;

void TabletPadStripV2Interface::sendFrame(quint32 time)
{
    d->send_frame(d->resourceForSurface(d->m_pad->currentSurface()), time);
}

void TabletPadStripV2Interface::sendPosition(quint32 position)
{
    d->send_position(d->resourceForSurface(d->m_pad->currentSurface()), position);
}

void TabletPadStripV2Interface::sendSource(Source source)
{
    d->send_source(d->resourceForSurface(d->m_pad->currentSurface()), source);
}

void TabletPadStripV2Interface::sendStop()
{
    d->send_stop(d->resourceForSurface(d->m_pad->currentSurface()));
}

class TabletPadGroupV2InterfacePrivate : public QtWaylandServer::zwp_tablet_pad_group_v2
{
public:
    TabletPadGroupV2InterfacePrivate(quint32 currentMode, TabletPadGroupV2Interface *q)
        : zwp_tablet_pad_group_v2()
        , q(q)
        , m_currentMode(currentMode)
    {
    }

    wl_resource *resourceForSurface(SurfaceInterface *surface) const
    {
        ClientConnection *client = surface->client();
        Resource *r = resourceMap().value(*client);
        return r ? r->handle : nullptr;
    }

    void zwp_tablet_pad_group_v2_destroy_resource(Resource *resource) override {
        Q_UNUSED(resource)
        if (m_pad->isRemoved() && resourceMap().isEmpty()) {
            delete q;
        }
    }

    void zwp_tablet_pad_group_v2_destroy(Resource *resource) override {
        wl_resource_destroy(resource->handle);
    }

    TabletPadGroupV2Interface *const q;
    TabletPadV2Interface *m_pad = nullptr;
    quint32 m_currentMode;
};

TabletPadGroupV2Interface::TabletPadGroupV2Interface(quint32 currentMode, TabletPadV2Interface *parent)
    : QObject(parent)
    , d(new TabletPadGroupV2InterfacePrivate(currentMode, this))
{
    d->m_pad = parent;
}

TabletPadGroupV2Interface::~TabletPadGroupV2Interface() = default;

void TabletPadGroupV2Interface::sendModeSwitch(quint32 time, quint32 serial, quint32 mode)
{
    d->m_currentMode = mode;
    d->send_mode_switch(d->resourceForSurface(d->m_pad->currentSurface()), time, serial, mode);
}

class TabletPadV2InterfacePrivate : public QtWaylandServer::zwp_tablet_pad_v2
{
public:
    TabletPadV2InterfacePrivate(const QString &path, quint32 buttons, quint32 rings, quint32 strips, quint32 modes, quint32 currentMode, Display *display, TabletPadV2Interface *q)
        : zwp_tablet_pad_v2()
        , q(q)
        , m_path(path)
        , m_buttons(buttons)
        , m_modes(modes)
        , m_padGroup(new TabletPadGroupV2Interface(currentMode, q))
        , m_display(display)
    {
        for (uint i = 0; i < buttons; ++i) {
            m_buttons[i] = i;
        }

        m_rings.reserve(rings);
        for (quint32 i = 0; i < rings; ++i) {
            m_rings += new TabletPadRingV2Interface(q);
        }

        m_strips.reserve(strips);
        for (quint32 i = 0; i < strips; ++i) {
            m_strips += new TabletPadStripV2Interface(q);
        }
    }

    void zwp_tablet_pad_v2_destroy_resource(Resource *resource) override {
        Q_UNUSED(resource)
        if (m_removed && resourceMap().isEmpty()) {
            delete q;
        }
    }

    void zwp_tablet_pad_v2_destroy(Resource *resource) override {
        wl_resource_destroy(resource->handle);
    }

    void zwp_tablet_pad_v2_set_feedback(Resource *resource, quint32 button, const QString &description, quint32 serial) override {
        Q_EMIT q->feedback(m_display->getConnection(resource->client()), button, description, serial);
    }

    wl_resource *resourceForSurface(SurfaceInterface *surface) const
    {
        ClientConnection *client = surface->client();
        Resource *r = resourceMap().value(*client);
        return r ? r->handle : nullptr;
    }

    TabletPadV2Interface *const q;

    const QString m_path;
    QVector<quint32> m_buttons;
    const int m_modes;

    QVector<TabletPadRingV2Interface *> m_rings;
    QVector<TabletPadStripV2Interface *> m_strips;
    TabletPadGroupV2Interface *const m_padGroup;
    TabletSeatV2Interface *m_seat = nullptr;
    SurfaceInterface *m_currentSurface = nullptr;
    bool m_removed = false;
    Display *const m_display;
};

TabletPadV2Interface::TabletPadV2Interface(const QString &path, quint32 buttons, quint32 rings, quint32 strips, quint32 modes, quint32 currentMode, Display *display, TabletSeatV2Interface *parent)
    : QObject(parent)
    , d(new TabletPadV2InterfacePrivate(path, buttons, rings, strips, modes, currentMode, display, this))
{
    d->m_seat = parent;
}

TabletPadV2Interface::~TabletPadV2Interface() = default;

void TabletPadV2Interface::sendButton(quint32 time, quint32 button, bool pressed)
{
    d->send_button(d->resourceForSurface(currentSurface()), time, button, pressed);
}

void TabletPadV2Interface::sendRemoved()
{
    d->m_removed = true;
    for (auto resource : d->resourceMap()) {
        d->send_removed(resource->handle);
    }
}

TabletPadRingV2Interface *TabletPadV2Interface::ring(uint at) const
{
    return d->m_rings[at];
}

TabletPadStripV2Interface *TabletPadV2Interface::strip(uint at) const
{
    return d->m_strips[at];
}

bool TabletPadV2Interface::isRemoved() const
{
    return d->m_removed;
}

void TabletPadV2Interface::setCurrentSurface(SurfaceInterface *surface, TabletV2Interface *tablet)
{
    if (surface == d->m_currentSurface) {
        return;
    }

    if (d->m_currentSurface) {
        d->send_leave(d->m_display->nextSerial(), surface->resource());
    }

    d->m_currentSurface = surface;
    if (surface) {
        wl_resource *tabletResource = tablet->d->resourceForSurface(surface);

        d->send_enter(d->resourceForSurface(surface), d->m_display->nextSerial(), tabletResource, surface->resource());
        d->m_padGroup->sendModeSwitch(0, d->m_display->nextSerial(), d->m_padGroup->d->m_currentMode);
    }
}

SurfaceInterface *TabletPadV2Interface::currentSurface() const
{
    return d->m_currentSurface;
}

class TabletSeatV2InterfacePrivate : public QtWaylandServer::zwp_tablet_seat_v2
{
public:
    TabletSeatV2InterfacePrivate(Display *display, TabletSeatV2Interface *q)
        : zwp_tablet_seat_v2()
        , q(q)
        , m_display(display)
    {
    }

    void zwp_tablet_seat_v2_bind_resource(Resource *resource) override
    {
        for (auto tablet : qAsConst(m_tablets)) {
            sendTabletAdded(resource, tablet);
        }

        for (auto pad : qAsConst(m_pads)) {
            sendPadAdded(resource, pad);
        }

        for (auto *tool : qAsConst(m_tools)) {
            sendToolAdded(resource, tool);
        }
    }

    void sendToolAdded(Resource *resource, TabletToolV2Interface *tool)
    {
        if (tool->d->m_removed) {
            return;
        }

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
        if (tablet->d->m_removed) {
            return;
        }

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

    void sendPadAdded(Resource *resource, TabletPadV2Interface *pad)
    {
        if (pad->d->m_removed) {
            return;
        }

        wl_resource *tabletResource = pad->d->add(resource->client(), resource->version())->handle;
        send_pad_added(resource->handle, tabletResource);

        pad->d->send_buttons(tabletResource, pad->d->m_buttons.size());
        pad->d->send_path(tabletResource, pad->d->m_path);

        auto groupResource = pad->d->m_padGroup->d->add(resource->client(), resource->version());
        pad->d->send_group(tabletResource, groupResource->handle);
        pad->d->m_padGroup->d->send_modes(groupResource->handle, pad->d->m_modes);

        pad->d->m_padGroup->d->send_buttons(groupResource->handle, QByteArray::fromRawData(reinterpret_cast<const char *>(pad->d->m_buttons.data()), pad->d->m_buttons.size() * sizeof(quint32)));

        for (auto ring : qAsConst(pad->d->m_rings)) {
            auto ringResource = ring->d->add(resource->client(), resource->version());
            pad->d->m_padGroup->d->send_ring(groupResource->handle, ringResource->handle);
        }

        for (auto strip : qAsConst(pad->d->m_strips)) {
            auto stripResource = strip->d->add(resource->client(), resource->version());
            pad->d->m_padGroup->d->send_strip(groupResource->handle, stripResource->handle);
        }
        pad->d->m_padGroup->d->send_done(groupResource->handle);
        pad->d->send_done(tabletResource);
    }

    TabletSeatV2Interface *const q;
    QVector<TabletToolV2Interface *> m_tools;
    QHash<QString, TabletV2Interface *> m_tablets;
    QHash<QString, TabletPadV2Interface *> m_pads;
    Display *const m_display;
};

TabletSeatV2Interface::TabletSeatV2Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new TabletSeatV2InterfacePrivate(display, this))
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
    Q_ASSERT(!d->m_tablets.contains(sysname));

    auto iface = new TabletV2Interface(vendorId, productId, name, paths, this);

    for (QtWaylandServer::zwp_tablet_seat_v2::Resource *r : d->resourceMap()) {
        d->sendTabletAdded(r, iface);
    }

    d->m_tablets[sysname] = iface;
    return iface;
}

TabletPadV2Interface *TabletSeatV2Interface::addTabletPad(const QString &sysname, const QString &name, const QStringList &paths, quint32 buttons, quint32 rings, quint32 strips, quint32 modes, quint32 currentMode, TabletV2Interface *tablet)
{
    Q_UNUSED(name);
    auto iface = new TabletPadV2Interface(paths.at(0), buttons, rings, strips, modes, currentMode, d->m_display, this);
    iface->d->m_seat = this;
    for (auto r : d->resourceMap()) {
        d->sendPadAdded(r, iface);
    }

    tablet->d->m_pad = iface;

    d->m_pads[sysname] = iface;
    return iface;
}

void TabletSeatV2Interface::removeDevice(const QString &sysname)
{
    auto tablet = d->m_tablets.take(sysname);
    if (tablet) {
        tablet->sendRemoved();
    }

    auto pad = d->m_pads.take(sysname);
    if (pad) {
        pad->sendRemoved();
    }
}

TabletToolV2Interface *TabletSeatV2Interface::toolByHardwareId(quint64 hardwareId) const
{
    for (TabletToolV2Interface *tool : qAsConst(d->m_tools)) {
        if (tool->d->hardwareId() == hardwareId) {
            return tool;
        }
    }
    return nullptr;
}

TabletToolV2Interface *TabletSeatV2Interface::toolByHardwareSerial(quint64 hardwareSerial) const
{
    for (TabletToolV2Interface *tool : qAsConst(d->m_tools)) {
        if (tool->d->hardwareSerial() == hardwareSerial)
            return tool;
    }
    return nullptr;
}

TabletPadV2Interface * TabletSeatV2Interface::padByName(const QString &name) const
{
    Q_ASSERT(d->m_pads.contains(name));
    return d->m_pads.value(name);
}

class TabletManagerV2InterfacePrivate : public QtWaylandServer::zwp_tablet_manager_v2
{
public:
    TabletManagerV2InterfacePrivate(Display *display, TabletManagerV2Interface *q)
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
    , d(new TabletManagerV2InterfacePrivate(display, this))
{
}

TabletSeatV2Interface *TabletManagerV2Interface::seat(SeatInterface *seat) const
{
    return d->get(seat);
}

bool TabletSeatV2Interface::isClientSupported(ClientConnection *client) const
{
    return d->resourceMap().value(*client);
}

TabletManagerV2Interface::~TabletManagerV2Interface() = default;

} // namespace KWaylandServer
