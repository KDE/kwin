/*
    SPDX-FileCopyrightText: 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "tablet_v2.h"
#include "clientconnection.h"
#include "core/inputdevice.h"
#include "display.h"
#include "seat.h"
#include "surface.h"
#include "utils/resource.h"

#include "qwayland-server-tablet-unstable-v2.h"

#include <QHash>
#include <QPointer>
#include <ranges>

namespace KWin
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

    TabletV2Interface *const q;
    const uint32_t m_vendorId;
    const uint32_t m_productId;
    const QString m_name;
    const QStringList m_paths;
};

TabletV2Interface::TabletV2Interface(uint32_t vendorId, uint32_t productId, const QString &name, const QStringList &paths, QObject *parent)
    : QObject(parent)
    , d(new TabletV2InterfacePrivate(this, vendorId, productId, name, paths))
{
}

TabletV2Interface::~TabletV2Interface()
{
    const auto tabletResources = d->resourceMap();
    for (TabletV2InterfacePrivate::Resource *resource : tabletResources) {
        d->send_removed(resource->handle);
    }
}

bool TabletV2Interface::isSurfaceSupported(SurfaceInterface *surface) const
{
    return d->resourceForSurface(surface);
}

class TabletSurfaceCursorV2Private
{
public:
    TabletSurfaceCursorV2Private(TabletSurfaceCursorV2 *q)
        : q(q)
    {
    }

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

    TabletSurfaceCursorV2 *const q;

    quint32 m_serial = 0;
    QPointer<SurfaceInterface> m_surface;
    QPoint m_hotspot;
};

TabletSurfaceCursorV2::TabletSurfaceCursorV2()
    : QObject()
    , d(new TabletSurfaceCursorV2Private(this))
{
}

TabletSurfaceCursorV2::~TabletSurfaceCursorV2() = default;

QPoint TabletSurfaceCursorV2::hotspot() const
{
    return d->m_hotspot;
}

quint32 TabletSurfaceCursorV2::enteredSerial() const
{
    return d->m_serial;
}

SurfaceInterface *TabletSurfaceCursorV2::surface() const
{
    return d->m_surface;
}

class TabletToolV2InterfacePrivate : public QtWaylandServer::zwp_tablet_tool_v2
{
public:
    TabletToolV2InterfacePrivate(TabletToolV2Interface *q,
                                 Display *display,
                                 TabletToolV2Interface::Type type,
                                 uint32_t hsh,
                                 uint32_t hsl,
                                 uint32_t hih,
                                 uint32_t hil,
                                 const QList<TabletToolV2Interface::Capability> &capabilities)
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

    std::ranges::subrange<QMultiMap<struct ::wl_client *, Resource *>::const_iterator> targetResources() const
    {
        if (!m_surface)
            return {};

        ClientConnection *client = m_surface->client();
        const auto [start, end] = resourceMap().equal_range(*client);
        return std::ranges::subrange(start, end);
    }

    quint64 hardwareId() const
    {
        return quint64(quint64(m_hardwareIdHigh) << 32) + m_hardwareIdLow;
    }
    quint64 hardwareSerial() const
    {
        return quint64(quint64(m_hardwareSerialHigh) << 32) + m_hardwareSerialLow;
    }

    void zwp_tablet_tool_v2_bind_resource(QtWaylandServer::zwp_tablet_tool_v2::Resource *resource) override
    {
        TabletSurfaceCursorV2 *&c = m_cursors[resource->client()];
        if (!c)
            c = new TabletSurfaceCursorV2;
    }

    void zwp_tablet_tool_v2_set_cursor(Resource *resource, uint32_t serial, struct ::wl_resource *_surface, int32_t hotspot_x, int32_t hotspot_y) override
    {
        SurfaceInterface *surface = SurfaceInterface::get(_surface);
        if (surface) {
            static SurfaceRole cursorRole(QByteArrayLiteral("tablet_cursor_v2"));
            if (const SurfaceRole *role = surface->role()) {
                if (role != &cursorRole) {
                    wl_resource_post_error(resource->handle, 0,
                                           "the wl_surface already has a role assigned %s", role->name().constData());
                    return;
                }
            } else {
                surface->setRole(&cursorRole);
            }
        }

        TabletSurfaceCursorV2 *c = m_cursors[resource->client()];
        c->d->update(serial, surface, {hotspot_x, hotspot_y});
        const auto resources = targetResources();
        if (std::any_of(resources.begin(), resources.end(), [resource](const Resource *res) {
                return res->handle == resource->handle;
            })) {
            Q_EMIT q->cursorChanged(c);
        }
    }

    void zwp_tablet_tool_v2_destroy_resource(Resource *resource) override
    {
        if (!resourceMap().contains(resource->client())) {
            delete m_cursors.take(resource->client());
        }
        if (m_removed && resourceMap().isEmpty()) {
            delete q;
        }
    }

    void zwp_tablet_tool_v2_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    Display *const m_display;
    quint32 m_proximitySerial = 0;
    std::optional<quint32> m_downSerial;
    bool m_cleanup = false;
    bool m_removed = false;
    QPointer<SurfaceInterface> m_surface;
    QPointer<TabletV2Interface> m_lastTablet;
    const uint32_t m_type;
    const uint32_t m_hardwareSerialHigh, m_hardwareSerialLow;
    const uint32_t m_hardwareIdHigh, m_hardwareIdLow;
    const QList<TabletToolV2Interface::Capability> m_capabilities;
    QHash<wl_client *, TabletSurfaceCursorV2 *> m_cursors;
    TabletToolV2Interface *const q;
};

TabletToolV2Interface::TabletToolV2Interface(Display *display,
                                             Type type,
                                             uint32_t hsh,
                                             uint32_t hsl,
                                             uint32_t hih,
                                             uint32_t hil,
                                             const QList<Capability> &capabilities)
    : d(new TabletToolV2InterfacePrivate(this, display, type, hsh, hsl, hih, hil, capabilities))
{
}

TabletToolV2Interface::~TabletToolV2Interface()
{
    const auto toolResources = d->resourceMap();
    for (TabletToolV2InterfacePrivate::Resource *resource : toolResources) {
        d->send_removed(resource->handle);
    }
}

TabletToolV2Interface *TabletToolV2Interface::get(wl_resource *resource)
{
    if (TabletToolV2InterfacePrivate *tabletToolPrivate = resource_cast<TabletToolV2InterfacePrivate *>(resource)) {
        return tabletToolPrivate->q;
    }
    return nullptr;
}

bool TabletToolV2Interface::hasCapability(Capability capability) const
{
    return d->m_capabilities.contains(capability);
}

SurfaceInterface *TabletToolV2Interface::currentSurface() const
{
    return d->m_surface;
}

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

    if (surface != nullptr) {
        if (auto *const cursor = d->m_cursors.value(*surface->client())) {
            Q_EMIT cursorChanged(cursor);
        }
    }
}

quint32 TabletToolV2Interface::proximitySerial() const
{
    return d->m_proximitySerial;
}

std::optional<quint32> TabletToolV2Interface::downSerial() const
{
    return d->m_downSerial;
}

bool TabletToolV2Interface::isClientSupported() const
{
    return d->m_surface && !d->targetResources().empty();
}

void TabletToolV2Interface::sendButton(uint32_t button, bool pressed)
{
    const auto serial = d->m_display->nextSerial();
    for (auto *resource : d->targetResources()) {
        d->send_button(resource->handle,
                       serial,
                       button,
                       pressed ? QtWaylandServer::zwp_tablet_tool_v2::button_state_pressed : QtWaylandServer::zwp_tablet_tool_v2::button_state_released);
    }
}

void TabletToolV2Interface::sendMotion(const QPointF &pos)
{
    const QPointF surfacePos = d->m_surface->toSurfaceLocal(pos);
    for (auto *resource : d->targetResources()) {
        d->send_motion(resource->handle, wl_fixed_from_double(surfacePos.x()), wl_fixed_from_double(surfacePos.y()));
    }
}

void TabletToolV2Interface::sendDistance(qreal distance)
{
    for (auto *resource : d->targetResources()) {
        d->send_distance(resource->handle, 65535 * distance);
    }
}

void TabletToolV2Interface::sendFrame(uint32_t time)
{
    for (auto *resource : d->targetResources()) {
        d->send_frame(resource->handle, time);
    }

    if (d->m_cleanup) {
        d->m_surface = nullptr;
        d->m_lastTablet = nullptr;
        d->m_cleanup = false;
    }
}

void TabletToolV2Interface::sendPressure(qreal pressure)
{
    for (auto *resource : d->targetResources()) {
        d->send_pressure(resource->handle, 65535 * pressure);
    }
}

void TabletToolV2Interface::sendRotation(qreal rotation)
{
    for (auto *resource : d->targetResources()) {
        d->send_rotation(resource->handle, wl_fixed_from_double(rotation));
    }
}

void TabletToolV2Interface::sendSlider(qreal position)
{
    for (auto *resource : d->targetResources()) {
        d->send_slider(resource->handle, 65535 * position);
    }
}

void TabletToolV2Interface::sendTilt(qreal degreesX, qreal degreesY)
{
    for (auto *resource : d->targetResources()) {
        d->send_tilt(resource->handle, wl_fixed_from_double(degreesX), wl_fixed_from_double(degreesY));
    }
}

void TabletToolV2Interface::sendWheel(int32_t degrees, int32_t clicks)
{
    for (auto *resource : d->targetResources()) {
        d->send_wheel(resource->handle, degrees, clicks);
    }
}

void TabletToolV2Interface::sendProximityIn(TabletV2Interface *tablet)
{
    wl_resource *tabletResource = tablet->d->resourceForSurface(d->m_surface);
    const auto serial = d->m_display->nextSerial();
    for (auto *resource : d->targetResources()) {
        d->send_proximity_in(resource->handle, serial, tabletResource, d->m_surface->resource());
    }
    d->m_proximitySerial = serial;
    d->m_lastTablet = tablet;
}

void TabletToolV2Interface::sendProximityOut()
{
    for (auto *resource : d->targetResources()) {
        d->send_proximity_out(resource->handle);
    }
    d->m_cleanup = true;
}

void TabletToolV2Interface::sendDown()
{
    const auto serial = d->m_display->nextSerial();
    for (auto *resource : d->targetResources()) {
        d->send_down(resource->handle, serial);
    }
    d->m_downSerial = serial;
}

void TabletToolV2Interface::sendUp()
{
    for (auto *resource : d->targetResources()) {
        d->send_up(resource->handle);
    }
    d->m_downSerial.reset();
}

class TabletPadRingV2InterfacePrivate : public QtWaylandServer::zwp_tablet_pad_ring_v2
{
public:
    TabletPadRingV2InterfacePrivate(TabletPadRingV2Interface *q)
        : zwp_tablet_pad_ring_v2()
        , q(q)
    {
    }

    std::ranges::subrange<QMultiMap<struct ::wl_client *, Resource *>::const_iterator> resourcesForSurface(SurfaceInterface *surface) const
    {
        ClientConnection *client = surface->client();
        const auto [start, end] = resourceMap().equal_range(*client);
        return std::ranges::subrange(start, end);
    }

    void zwp_tablet_pad_ring_v2_destroy(Resource *resource) override
    {
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
    for (auto *resource : d->resourcesForSurface(d->m_pad->currentSurface())) {
        d->send_angle(resource->handle, wl_fixed_from_double(angle));
    }
}

void TabletPadRingV2Interface::sendFrame(quint32 time)
{
    for (auto *resource : d->resourcesForSurface(d->m_pad->currentSurface())) {
        d->send_frame(resource->handle, time);
    }
}

void TabletPadRingV2Interface::sendSource(Source source)
{
    for (auto *resource : d->resourcesForSurface(d->m_pad->currentSurface())) {
        d->send_source(resource->handle, source);
    }
}

void TabletPadRingV2Interface::sendStop()
{
    for (auto *resource : d->resourcesForSurface(d->m_pad->currentSurface())) {
        d->send_stop(resource->handle);
    }
}

class TabletPadStripV2InterfacePrivate : public QtWaylandServer::zwp_tablet_pad_strip_v2
{
public:
    TabletPadStripV2InterfacePrivate(TabletPadStripV2Interface *q)
        : zwp_tablet_pad_strip_v2()
        , q(q)
    {
    }

    std::ranges::subrange<QMultiMap<struct ::wl_client *, Resource *>::const_iterator> resourcesForSurface(SurfaceInterface *surface) const
    {
        ClientConnection *client = surface->client();
        const auto [start, end] = resourceMap().equal_range(*client);
        return std::ranges::subrange(start, end);
    }

    void zwp_tablet_pad_strip_v2_destroy(Resource *resource) override
    {
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
    for (auto *resource : d->resourcesForSurface(d->m_pad->currentSurface())) {
        d->send_frame(resource->handle, time);
    }
}

void TabletPadStripV2Interface::sendPosition(quint32 position)
{
    for (auto *resource : d->resourcesForSurface(d->m_pad->currentSurface())) {
        d->send_position(resource->handle, position);
    }
}

void TabletPadStripV2Interface::sendSource(Source source)
{
    for (auto *resource : d->resourcesForSurface(d->m_pad->currentSurface())) {
        d->send_source(resource->handle, source);
    }
}

void TabletPadStripV2Interface::sendStop()
{
    for (auto *resource : d->resourcesForSurface(d->m_pad->currentSurface())) {
        d->send_stop(resource->handle);
    }
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

    std::ranges::subrange<QMultiMap<struct ::wl_client *, Resource *>::const_iterator> resourcesForSurface(SurfaceInterface *surface) const
    {
        ClientConnection *client = surface->client();
        const auto [start, end] = resourceMap().equal_range(*client);
        return std::ranges::subrange(start, end);
    }

    void zwp_tablet_pad_group_v2_destroy(Resource *resource) override
    {
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
    for (auto *resource : d->resourcesForSurface(d->m_pad->currentSurface())) {
        d->send_mode_switch(resource->handle, time, serial, mode);
    }
}

class TabletPadV2InterfacePrivate : public QtWaylandServer::zwp_tablet_pad_v2
{
public:
    TabletPadV2InterfacePrivate(const QString &path,
                                quint32 buttons,
                                quint32 rings,
                                quint32 strips,
                                quint32 modes,
                                quint32 currentMode,
                                Display *display,
                                TabletPadV2Interface *q)
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

    ~TabletPadV2InterfacePrivate() override
    {
        qDeleteAll(m_rings);
        qDeleteAll(m_strips);
    }

    void zwp_tablet_pad_v2_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void zwp_tablet_pad_v2_set_feedback(Resource *resource, quint32 button, const QString &description, quint32 serial) override
    {
        Q_EMIT q->feedback(m_display->getConnection(resource->client()), button, description, serial);
    }

    std::ranges::subrange<QMultiMap<struct ::wl_client *, Resource *>::const_iterator> resourcesForSurface(SurfaceInterface *surface) const
    {
        ClientConnection *client = surface->client();
        const auto [start, end] = resourceMap().equal_range(*client);
        return std::ranges::subrange(start, end);
    }

    TabletPadV2Interface *const q;

    const QString m_path;
    QList<quint32> m_buttons;
    const int m_modes;

    QList<TabletPadRingV2Interface *> m_rings;
    QList<TabletPadStripV2Interface *> m_strips;
    TabletPadGroupV2Interface *const m_padGroup;
    TabletSeatV2Interface *m_seat = nullptr;
    QPointer<SurfaceInterface> m_currentSurface;
    Display *const m_display;
};

TabletPadV2Interface::TabletPadV2Interface(const QString &path,
                                           quint32 buttons,
                                           quint32 rings,
                                           quint32 strips,
                                           quint32 modes,
                                           quint32 currentMode,
                                           Display *display,
                                           TabletSeatV2Interface *parent)
    : QObject(parent)
    , d(new TabletPadV2InterfacePrivate(path, buttons, rings, strips, modes, currentMode, display, this))
{
    d->m_seat = parent;
}

TabletPadV2Interface::~TabletPadV2Interface()
{
    const auto tabletPadResources = d->resourceMap();
    for (TabletPadV2InterfacePrivate::Resource *resource : tabletPadResources) {
        d->send_removed(resource->handle);
    }
}

void TabletPadV2Interface::sendButton(std::chrono::microseconds time, quint32 button, bool pressed)
{
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
    for (auto *resource : d->resourcesForSurface(currentSurface())) {
        d->send_button(resource->handle, milliseconds, button, pressed);
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

void TabletPadV2Interface::setCurrentSurface(SurfaceInterface *surface, TabletV2Interface *tablet)
{
    if (surface == d->m_currentSurface) {
        return;
    }

    if (d->m_currentSurface) {
        auto serial = d->m_display->nextSerial();
        for (auto *resource : d->resourcesForSurface(d->m_currentSurface)) {
            d->send_leave(resource->handle, serial, d->m_currentSurface->resource());
        }
    }

    d->m_currentSurface = surface;
    if (surface) {
        wl_resource *tabletResource = tablet->d->resourceForSurface(surface);

        auto serial = d->m_display->nextSerial();
        for (auto *resource : d->resourcesForSurface(surface)) {
            d->send_enter(resource->handle, serial, tabletResource, surface->resource());
        }
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
        for (auto tablet : std::as_const(m_tablets)) {
            sendTabletAdded(resource, tablet);
        }

        for (auto pad : std::as_const(m_pads)) {
            sendPadAdded(resource, pad);
        }

        for (auto tool : std::as_const(m_tools)) {
            sendToolAdded(resource, tool);
        }
    }

    void zwp_tablet_seat_v2_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void sendToolAdded(Resource *resource, TabletToolV2Interface *tool)
    {
        wl_resource *toolResource = tool->d->add(resource->client(), resource->version())->handle;
        send_tool_added(resource->handle, toolResource);

        tool->d->send_type(toolResource, tool->d->m_type);
        tool->d->send_hardware_serial(toolResource, tool->d->m_hardwareSerialHigh, tool->d->m_hardwareSerialLow);
        tool->d->send_hardware_id_wacom(toolResource, tool->d->m_hardwareIdHigh, tool->d->m_hardwareIdLow);
        for (uint32_t cap : std::as_const(tool->d->m_capabilities)) {
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
        for (const QString &path : std::as_const(tablet->d->m_paths)) {
            tablet->d->send_path(tabletResource, path);
        }
        tablet->d->send_done(tabletResource);
    }

    void sendPadAdded(Resource *resource, TabletPadV2Interface *pad)
    {
        wl_resource *tabletResource = pad->d->add(resource->client(), resource->version())->handle;
        send_pad_added(resource->handle, tabletResource);

        pad->d->send_buttons(tabletResource, pad->d->m_buttons.size());
        pad->d->send_path(tabletResource, pad->d->m_path);

        auto groupResource = pad->d->m_padGroup->d->add(resource->client(), resource->version());
        pad->d->send_group(tabletResource, groupResource->handle);
        pad->d->m_padGroup->d->send_modes(groupResource->handle, pad->d->m_modes);

        pad->d->m_padGroup->d->send_buttons(
            groupResource->handle,
            QByteArray::fromRawData(reinterpret_cast<const char *>(pad->d->m_buttons.data()), pad->d->m_buttons.size() * sizeof(quint32)));

        for (auto ring : std::as_const(pad->d->m_rings)) {
            auto ringResource = ring->d->add(resource->client(), resource->version());
            pad->d->m_padGroup->d->send_ring(groupResource->handle, ringResource->handle);
        }

        for (auto strip : std::as_const(pad->d->m_strips)) {
            auto stripResource = strip->d->add(resource->client(), resource->version());
            pad->d->m_padGroup->d->send_strip(groupResource->handle, stripResource->handle);
        }
        pad->d->m_padGroup->d->send_done(groupResource->handle);
        pad->d->send_done(tabletResource);
    }

    TabletSeatV2Interface *const q;
    QHash<InputDeviceTabletTool *, TabletToolV2Interface *> m_tools;
    QHash<InputDevice *, TabletV2Interface *> m_tablets;
    QHash<InputDevice *, TabletPadV2Interface *> m_pads;
    Display *const m_display;
};

TabletSeatV2Interface::TabletSeatV2Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new TabletSeatV2InterfacePrivate(display, this))
{
}

TabletSeatV2Interface::~TabletSeatV2Interface() = default;

TabletToolV2Interface *TabletSeatV2Interface::addTool(InputDeviceTabletTool *device)
{
    TabletToolV2Interface::Type type = TabletToolV2Interface::Type::Pen;
    switch (device->type()) {
    case InputDeviceTabletTool::Pen:
        type = TabletToolV2Interface::Pen;
        break;
    case InputDeviceTabletTool::Eraser:
        type = TabletToolV2Interface::Eraser;
        break;
    case InputDeviceTabletTool::Brush:
        type = TabletToolV2Interface::Brush;
        break;
    case InputDeviceTabletTool::Pencil:
        type = TabletToolV2Interface::Pencil;
        break;
    case InputDeviceTabletTool::Airbrush:
        type = TabletToolV2Interface::Airbrush;
        break;
    case InputDeviceTabletTool::Finger:
        type = TabletToolV2Interface::Finger;
        break;
    case InputDeviceTabletTool::Mouse:
        type = TabletToolV2Interface::Mouse;
        break;
    case InputDeviceTabletTool::Lens:
        type = TabletToolV2Interface::Lens;
        break;
    case InputDeviceTabletTool::Totem:
        type = TabletToolV2Interface::Totem;
        break;
    }

    QList<TabletToolV2Interface::Capability> capabilities;
    const QList<InputDeviceTabletTool::Capability> deviceCapabilities = device->capabilities();
    for (const InputDeviceTabletTool::Capability &deviceCapability : deviceCapabilities) {
        TabletToolV2Interface::Capability capability = TabletToolV2Interface::Capability::Wheel;
        switch (deviceCapability) {
        case InputDeviceTabletTool::Tilt:
            capability = TabletToolV2Interface::Capability::Tilt;
            break;
        case InputDeviceTabletTool::Pressure:
            capability = TabletToolV2Interface::Capability::Pressure;
            break;
        case InputDeviceTabletTool::Distance:
            capability = TabletToolV2Interface::Capability::Distance;
            break;
        case InputDeviceTabletTool::Rotation:
            capability = TabletToolV2Interface::Capability::Rotation;
            break;
        case InputDeviceTabletTool::Slider:
            capability = TabletToolV2Interface::Capability::Slider;
            break;
        case InputDeviceTabletTool::Wheel:
            capability = TabletToolV2Interface::Capability::Wheel;
            break;
        }
        capabilities.append(capability);
    }

    constexpr auto MAX_UINT_32 = std::numeric_limits<quint32>::max();
    auto tool = new TabletToolV2Interface(d->m_display,
                                          type,
                                          device->serialId() >> 32,
                                          device->serialId() & MAX_UINT_32,
                                          device->uniqueId() >> 32,
                                          device->uniqueId() & MAX_UINT_32,
                                          capabilities);
    for (QtWaylandServer::zwp_tablet_seat_v2::Resource *resource : d->resourceMap()) {
        d->sendToolAdded(resource, tool);
    }

    d->m_tools[device] = tool;
    return tool;
}

TabletV2Interface *
TabletSeatV2Interface::addTablet(InputDevice *device)
{
    Q_ASSERT(!d->m_tablets.contains(device));

    auto iface = new TabletV2Interface(device->vendor(), device->product(), device->name(), {device->sysPath()}, this);

    for (QtWaylandServer::zwp_tablet_seat_v2::Resource *r : d->resourceMap()) {
        d->sendTabletAdded(r, iface);
    }

    d->m_tablets[device] = iface;
    return iface;
}

TabletPadV2Interface *TabletSeatV2Interface::addPad(InputDevice *device)
{
    auto iface = new TabletPadV2Interface(device->sysPath(), device->tabletPadButtonCount(), device->tabletPadRingCount(), device->tabletPadStripCount(), device->tabletPadModeCount(), device->tabletPadMode(), d->m_display, this);
    iface->d->m_seat = this;
    for (auto r : d->resourceMap()) {
        d->sendPadAdded(r, iface);
    }

    d->m_pads[device] = iface;
    return iface;
}

void TabletSeatV2Interface::remove(InputDevice *device)
{
    delete d->m_tablets.take(device);
    delete d->m_pads.take(device);
}

TabletToolV2Interface *TabletSeatV2Interface::tool(InputDeviceTabletTool *device) const
{
    return d->m_tools.value(device);
}

TabletV2Interface *TabletSeatV2Interface::tablet(InputDevice *device) const
{
    return d->m_tablets.value(device);
}

TabletPadV2Interface *TabletSeatV2Interface::pad(InputDevice *device) const
{
    return d->m_pads.value(device);
}

TabletV2Interface *TabletSeatV2Interface::matchingTablet(TabletPadV2Interface *pad) const
{
    InputDevice *padDevice = d->m_pads.key(pad);
    if (!padDevice) {
        return nullptr;
    }

    for (auto it = d->m_tablets.constBegin(); it != d->m_tablets.constEnd(); ++it) {
        if (padDevice->group() == it.key()->group()) {
            return it.value();
        }
    }

    return nullptr;
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

    void zwp_tablet_manager_v2_get_tablet_seat(Resource *resource, uint32_t tablet_seat, struct ::wl_resource *seat_resource) override
    {
        SeatInterface *seat = SeatInterface::get(seat_resource);
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

bool TabletSeatV2Interface::hasImplicitGrab(quint32 serial) const
{
    return std::any_of(d->m_tools.cbegin(), d->m_tools.cend(), [serial](const auto &tool) {
        return tool->downSerial() == serial;
    });
}

TabletManagerV2Interface::~TabletManagerV2Interface() = default;

} // namespace KWin

#include "moc_tablet_v2.cpp"
