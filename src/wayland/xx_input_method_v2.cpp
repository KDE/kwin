/*
    SPDX-FileCopyrightText: 2026 KWin

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "xx_input_method_v2.h"

#include "utils/resource.h"
#include "wayland/display.h"
#include "wayland/seat.h"
#include "wayland/surface.h"

#include "qwayland-server-text-input-unstable-v3.h"
#include "qwayland-server-xx-input-method-v2.h"

namespace KWin
{

static const int s_managerVersion = 2;
static const int s_inputMethodVersion = 2;
static const int s_popupVersion = 1;

class XXInputPopupSurfaceV2InterfacePrivate : public QtWaylandServer::xx_input_popup_surface_v2
{
public:
    explicit XXInputPopupSurfaceV2InterfacePrivate(XXInputPopupSurfaceV2Interface *q, SurfaceInterface *surface, wl_resource *resource)
        : q(q)
        , surface(surface)
    {
        init(resource);
    }

    void xx_input_popup_surface_v2_destroy_resource(Resource *) override
    {
        Q_EMIT q->aboutToBeDestroyed();
        delete q;
    }

    void xx_input_popup_surface_v2_ack_configure(Resource *resource, uint32_t serial) override
    {
        Q_EMIT q->ackConfigureReceived(serial);
    }

    XXInputPopupSurfaceV2Interface *const q;
    QPointer<SurfaceInterface> surface;
};

XXInputPopupSurfaceV2Interface::XXInputPopupSurfaceV2Interface(SurfaceInterface *surface, wl_resource *resource, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<XXInputPopupSurfaceV2InterfacePrivate>(this, surface, resource))
{
}

XXInputPopupSurfaceV2Interface::~XXInputPopupSurfaceV2Interface() = default;

SurfaceRole *XXInputPopupSurfaceV2Interface::role()
{
    static SurfaceRole role(QByteArrayLiteral("xx_input_popup_surface_v2"));
    return &role;
}

SurfaceInterface *XXInputPopupSurfaceV2Interface::surface() const
{
    return d->surface;
}

quint32 XXInputPopupSurfaceV2Interface::sendStartConfigure(uint32_t width, uint32_t height,
                                                           int32_t anchorX, int32_t anchorY,
                                                           uint32_t anchorW, uint32_t anchorH)
{
    // The serial for the configure sequence is sent as part of start_configure
    // The corresponding input_method.done should be sent by the owner of this popup
    const quint32 serial = d->display()->nextSerial();
    d->send_start_configure(width, height, anchorX, anchorY, anchorW, anchorH, serial);
    return serial;
}

class XXInputMethodV1InterfacePrivate : public QtWaylandServer::xx_input_method_v1
{
public:
    XXInputMethodV1InterfacePrivate(XXInputMethodV1Interface *q, Display *display, SeatInterface *seat, wl_resource *resource)
        : q(q)
        , display(display)
        , seat(seat)
    {
        init(resource);
    }

    void xx_input_method_v1_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void xx_input_method_v1_commit_string(Resource *, const QString &text) override
    {
        Q_EMIT q->commitString(text);
    }

    void xx_input_method_v1_set_preedit_string(Resource *, const QString &text, int32_t cursor_begin, int32_t cursor_end) override
    {
        Q_EMIT q->setPreeditString(text, cursor_begin, cursor_end);
    }

    void xx_input_method_v1_delete_surrounding_text(Resource *, uint32_t before_length, uint32_t after_length) override
    {
        Q_EMIT q->deleteSurroundingText(before_length, after_length);
    }

    void xx_input_method_v1_commit(Resource *, uint32_t serial) override
    {
        Q_EMIT q->commit(serial);
    }

    void xx_input_method_v1_get_input_popup_surface(Resource *resource, uint32_t id, struct ::wl_resource *surfaceResource, struct ::wl_resource *positioner) override
    {
        SurfaceInterface *surface = SurfaceInterface::get(surfaceResource);
        if (const SurfaceRole *existing = surface->role()) {
            if (existing != XXInputPopupSurfaceV2Interface::role()) {
                wl_resource_post_error(resource->handle, error_surface_has_role, "the surface already has a role assigned %s", existing->name().constData());
                return;
            }
        } else {
            surface->setRole(XXInputPopupSurfaceV2Interface::role());
        }

        wl_resource *popupRes = wl_resource_create(resource->client(), &xx_input_popup_surface_v2_interface, resource->version(), id);
        auto popup = new XXInputPopupSurfaceV2Interface(surface, popupRes);
        Q_EMIT q->inputPopupSurfaceAdded(popup);
    }

    void send_activate()
    {
        send_activate();
    }

    XXInputMethodV1Interface *const q;
    Display *const display;
    SeatInterface *const seat;
};

XXInputMethodV1Interface::XXInputMethodV1Interface(Display *display, SeatInterface *seat, wl_resource *resource)
    : d(std::make_unique<XXInputMethodV1InterfacePrivate>(this, display, seat, resource))
{
}

XXInputMethodV1Interface::~XXInputMethodV1Interface() = default;

void XXInputMethodV1Interface::sendActivate()
{
    d->send_activate();
}

void XXInputMethodV1Interface::sendDeactivate()
{
    d->send_deactivate();
}

void XXInputMethodV1Interface::sendSurroundingText(const QString &text, quint32 cursor, quint32 anchor)
{
    d->send_surrounding_text(text, cursor, anchor);
}

void XXInputMethodV1Interface::sendContentType(uint32_t hint, uint32_t purpose)
{
    d->send_content_type(hint, purpose);
}

void XXInputMethodV1Interface::sendDone()
{
    d->send_done();
}

class XXInputMethodManagerV2InterfacePrivate : public QtWaylandServer::xx_input_method_manager_v2
{
public:
    explicit XXInputMethodManagerV2InterfacePrivate(XXInputMethodManagerV2Interface *q, Display *display)
        : QtWaylandServer::xx_input_method_manager_v2(*display, s_managerVersion)
        , q(q)
        , display(display)
    {
    }

    void xx_input_method_manager_v2_destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void xx_input_method_manager_v2_get_input_method(Resource *resource, struct ::wl_resource *seat_resource, uint32_t id) override
    {
        auto seat = SeatInterface::get(seat_resource);
        wl_resource *imRes = wl_resource_create(resource->client(), &xx_input_method_v1_interface, s_inputMethodVersion, id);
        auto im = new XXInputMethodV1Interface(display, seat, imRes);
        Q_EMIT q->inputMethodCreated(im);
    }

    void xx_input_method_manager_v2_get_positioner(Resource *resource, uint32_t /*id*/) override
    {
        // Not used currently by the server side; client uses it as request argument.
    }

    XXInputMethodManagerV2Interface *q;
    Display *display;
};

XXInputMethodManagerV2Interface::XXInputMethodManagerV2Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<XXInputMethodManagerV2InterfacePrivate>(this, display))
{
}

XXInputMethodManagerV2Interface::~XXInputMethodManagerV2Interface() = default;

} // namespace KWin

#include "moc_xx_input_method_v2.cpp"
