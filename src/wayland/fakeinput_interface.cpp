/*
    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "fakeinput_interface.h"
#include "display.h"

#include <QSizeF>
#include <QPointF>

#include <wayland-server.h>
#include <qwayland-server-fake-input.h>


namespace KWaylandServer
{

static const quint32 s_version = 4;

class FakeInputInterfacePrivate : public QtWaylandServer::org_kde_kwin_fake_input
{
public:
    FakeInputInterfacePrivate(FakeInputInterface *_q, Display *display);
    QList<FakeInputDevice*> devices;

private:
    FakeInputDevice *device(wl_resource *r);

    FakeInputInterface *q;
    static QList<quint32> touchIds;

protected:
    void org_kde_kwin_fake_input_bind_resource(Resource *resource) override;
    void org_kde_kwin_fake_input_destroy_resource(Resource *resource) override;
    void org_kde_kwin_fake_input_authenticate(Resource *resource, const QString &application, const QString &reason) override;
    void org_kde_kwin_fake_input_pointer_motion(Resource *resource, wl_fixed_t delta_x, wl_fixed_t delta_y) override;
    void org_kde_kwin_fake_input_button(Resource *resource, uint32_t button, uint32_t state) override;
    void org_kde_kwin_fake_input_axis(Resource *resource, uint32_t axis, wl_fixed_t value) override;
    void org_kde_kwin_fake_input_touch_down(Resource *resource, uint32_t id, wl_fixed_t x, wl_fixed_t y) override;
    void org_kde_kwin_fake_input_touch_motion(Resource *resource, uint32_t id, wl_fixed_t x, wl_fixed_t y) override;
    void org_kde_kwin_fake_input_touch_up(Resource *resource, uint32_t id) override;
    void org_kde_kwin_fake_input_touch_cancel(Resource *resource) override;
    void org_kde_kwin_fake_input_touch_frame(Resource *resource) override;
    void org_kde_kwin_fake_input_pointer_motion_absolute(Resource *resource, wl_fixed_t x, wl_fixed_t y) override;
    void org_kde_kwin_fake_input_keyboard_key(Resource *resource, uint32_t button, uint32_t state) override;
};

QList<quint32> FakeInputInterfacePrivate::touchIds = QList<quint32>();

FakeInputInterfacePrivate::FakeInputInterfacePrivate(FakeInputInterface *_q, Display *display)
    : QtWaylandServer::org_kde_kwin_fake_input(*display, s_version)
    , q(_q)
{
}

FakeInputInterface::FakeInputInterface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new FakeInputInterfacePrivate(this, display))
{
}

FakeInputInterface::~FakeInputInterface() = default;

void FakeInputInterfacePrivate::org_kde_kwin_fake_input_bind_resource(Resource *resource)
{
    FakeInputDevice *device = new FakeInputDevice(q, resource->handle);
    devices << device;
    QObject::connect(device, &FakeInputDevice::destroyed, q, [device, this] { devices.removeAll(device); });
    emit q->deviceCreated(device);
}

void FakeInputInterfacePrivate::org_kde_kwin_fake_input_destroy_resource(Resource *resource)
{
    if (FakeInputDevice *d = device(resource->handle)) {
        d->deleteLater();
    }
}

FakeInputDevice *FakeInputInterfacePrivate::device(wl_resource *r)
{
    auto it = std::find_if(devices.constBegin(), devices.constEnd(), [r] (FakeInputDevice *device) { return device->resource() == r; } );
    if (it != devices.constEnd()) {
        return *it;
    }
    return nullptr;
}

void FakeInputInterfacePrivate::org_kde_kwin_fake_input_authenticate(Resource *resource, const QString &application, const QString &reason)
{
    FakeInputDevice *d = device(resource->handle);
    if (!d) {
        return;
    }
    emit d->authenticationRequested(application, reason);
}

void FakeInputInterfacePrivate::org_kde_kwin_fake_input_pointer_motion(Resource *resource, wl_fixed_t delta_x, wl_fixed_t delta_y)
{
    FakeInputDevice *d = device(resource->handle);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    emit d->pointerMotionRequested(QSizeF(wl_fixed_to_double(delta_x), wl_fixed_to_double(delta_y)));
}

void FakeInputInterfacePrivate::org_kde_kwin_fake_input_button(Resource *resource, uint32_t button, uint32_t state)
{
    FakeInputDevice *d = device(resource->handle);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    switch (state) {
    case WL_POINTER_BUTTON_STATE_PRESSED:
        emit d->pointerButtonPressRequested(button);
        break;
    case WL_POINTER_BUTTON_STATE_RELEASED:
        emit d->pointerButtonReleaseRequested(button);
        break;
    default:
        // nothing
        break;
    }
}
void FakeInputInterfacePrivate::org_kde_kwin_fake_input_axis(Resource *resource, uint32_t axis, wl_fixed_t value)
{
    FakeInputDevice *d = device(resource->handle);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    Qt::Orientation orientation;
    switch (axis) {
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
        orientation = Qt::Horizontal;
        break;
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
        orientation = Qt::Vertical;
        break;
    default:
        // invalid
        return;
    }
    emit d->pointerAxisRequested(orientation, wl_fixed_to_double(value));
}

void FakeInputInterfacePrivate::org_kde_kwin_fake_input_touch_down(Resource *resource, uint32_t id, wl_fixed_t x, wl_fixed_t y)
{
    FakeInputDevice *d = device(resource->handle);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    if (touchIds.contains(id)) {
        return;
    }
    touchIds << id;
    emit d->touchDownRequested(id, QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
}

void FakeInputInterfacePrivate::org_kde_kwin_fake_input_touch_motion(Resource *resource, uint32_t id, wl_fixed_t x, wl_fixed_t y)
{
    FakeInputDevice *d = device(resource->handle);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    if (!touchIds.contains(id)) {
        return;
    }
    emit d->touchMotionRequested(id, QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
}

void FakeInputInterfacePrivate::org_kde_kwin_fake_input_touch_up(Resource *resource, uint32_t id)
{
    FakeInputDevice *d = device(resource->handle);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    if (!touchIds.contains(id)) {
        return;
    }
    touchIds.removeOne(id);
    emit d->touchUpRequested(id);
}

void FakeInputInterfacePrivate::org_kde_kwin_fake_input_touch_cancel(Resource *resource)
{
    FakeInputDevice *d = device(resource->handle);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    touchIds.clear();
    emit d->touchCancelRequested();
}

void FakeInputInterfacePrivate::org_kde_kwin_fake_input_touch_frame(Resource *resource)
{
    FakeInputDevice *d = device(resource->handle);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    emit d->touchFrameRequested();
}

void FakeInputInterfacePrivate::org_kde_kwin_fake_input_pointer_motion_absolute(Resource *resource, wl_fixed_t x, wl_fixed_t y)
{
    FakeInputDevice *d = device(resource->handle);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    emit d->pointerMotionAbsoluteRequested(QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
}

void FakeInputInterfacePrivate::org_kde_kwin_fake_input_keyboard_key(Resource *resource, uint32_t button, uint32_t state)
{
    FakeInputDevice *d = device(resource->handle);
    if (!d || !d->isAuthenticated()) {
        return;
    }
    switch (state) {
    case WL_KEYBOARD_KEY_STATE_PRESSED:
        emit d->keyboardKeyPressRequested(button);
        break;
    case WL_KEYBOARD_KEY_STATE_RELEASED:
        emit d->keyboardKeyReleaseRequested(button);
        break;
    default:
        // nothing
        break;
    }
}

class FakeInputDevicePrivate
{
public:
    FakeInputDevicePrivate(FakeInputInterface *interface, wl_resource *resource);
    wl_resource *resource;
    FakeInputInterface *interface;
    bool authenticated = false;
};

FakeInputDevicePrivate::FakeInputDevicePrivate(FakeInputInterface *interface, wl_resource *resource)
    : resource(resource)
    , interface(interface)
{
}

FakeInputDevice::FakeInputDevice(FakeInputInterface *parent, wl_resource *resource)
    : QObject(parent)
    , d(new FakeInputDevicePrivate(parent, resource))
{
}

FakeInputDevice::~FakeInputDevice() = default;

void FakeInputDevice::setAuthentication(bool authenticated)
{
    d->authenticated = authenticated;
}

wl_resource *FakeInputDevice::resource()
{
    return d->resource;
}

bool FakeInputDevice::isAuthenticated() const
{
    return d->authenticated;
}

}
