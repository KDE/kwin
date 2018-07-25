/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
Copyright 2015  Sebastian Kügler <sebas@kde.org>

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
#include "outputconfiguration_interface.h"
#include "outputdevice_interface.h"
#include "logging_p.h"
#include "resource_p.h"
#include "display.h"
#include "outputchangeset_p.h"

#include <wayland-server.h>
#include "wayland-output-management-server-protocol.h"
#include "wayland-org_kde_kwin_outputdevice-server-protocol.h"

#include <QDebug>
#include <QSize>

namespace KWayland
{
namespace Server
{


class OutputConfigurationInterface::Private : public Resource::Private
{
public:
    Private(OutputConfigurationInterface *q, OutputManagementInterface *c, wl_resource *parentResource);
    ~Private();

    void sendApplied();
    void sendFailed();
    void emitConfigurationChangeRequested() const;
    void clearPendingChanges();

    bool hasPendingChanges(OutputDeviceInterface *outputdevice) const;
    OutputChangeSet* pendingChanges(OutputDeviceInterface *outputdevice);

    OutputManagementInterface *outputManagement;
    QHash<OutputDeviceInterface*, OutputChangeSet*> changes;

    static const quint32 s_version = 2;

private:
    static void enableCallback(wl_client *client, wl_resource *resource,
                               wl_resource * outputdevice, int32_t enable);
    static void modeCallback(wl_client *client, wl_resource *resource,
                             wl_resource * outputdevice, int32_t mode_id);
    static void transformCallback(wl_client *client, wl_resource *resource,
                                  wl_resource * outputdevice, int32_t transform);
    static void positionCallback(wl_client *client, wl_resource *resource,
                                 wl_resource * outputdevice, int32_t x, int32_t y);
    static void scaleCallback(wl_client *client, wl_resource *resource,
                              wl_resource * outputdevice, int32_t scale);
    static void applyCallback(wl_client *client, wl_resource *resource);
    static void scaleFCallback(wl_client *client, wl_resource *resource,
                              wl_resource * outputdevice, wl_fixed_t scale);

    OutputConfigurationInterface *q_func() {
        return reinterpret_cast<OutputConfigurationInterface *>(q);
    }

    static const struct org_kde_kwin_outputconfiguration_interface s_interface;
};

const struct org_kde_kwin_outputconfiguration_interface OutputConfigurationInterface::Private::s_interface = {
    enableCallback,
    modeCallback,
    transformCallback,
    positionCallback,
    scaleCallback,
    applyCallback,
    scaleFCallback,
    resourceDestroyedCallback
};

OutputConfigurationInterface::OutputConfigurationInterface(OutputManagementInterface* parent, wl_resource* parentResource): Resource(new Private(this, parent, parentResource))
{
    Q_D();
    d->outputManagement = parent;
}

OutputConfigurationInterface::~OutputConfigurationInterface()
{
    Q_D();
    d->clearPendingChanges();
}

void OutputConfigurationInterface::Private::enableCallback(wl_client *client, wl_resource *resource, wl_resource * outputdevice, int32_t enable)
{
    Q_UNUSED(client);
    auto s = cast<Private>(resource);
    Q_ASSERT(s);
    auto _enable = (enable == ORG_KDE_KWIN_OUTPUTDEVICE_ENABLEMENT_ENABLED) ?
                                    OutputDeviceInterface::Enablement::Enabled :
                                    OutputDeviceInterface::Enablement::Disabled;
    OutputDeviceInterface *o = OutputDeviceInterface::get(outputdevice);
    s->pendingChanges(o)->d_func()->enabled = _enable;
}

void OutputConfigurationInterface::Private::modeCallback(wl_client *client, wl_resource *resource, wl_resource * outputdevice, int32_t mode_id)
{
    Q_UNUSED(client);
    OutputDeviceInterface *o = OutputDeviceInterface::get(outputdevice);

    bool modeValid = false;
    for (const auto &m: o->modes()) {
        if (m.id == mode_id) {
            modeValid = true;
            break;
        }
    }
    if (!modeValid) {
        qCWarning(KWAYLAND_SERVER) << "Set invalid mode id:" << mode_id;
        return;
    }
    auto s = cast<Private>(resource);
    Q_ASSERT(s);
    s->pendingChanges(o)->d_func()->modeId = mode_id;
}

void OutputConfigurationInterface::Private::transformCallback(wl_client *client, wl_resource *resource, wl_resource * outputdevice, int32_t transform)
{
    Q_UNUSED(client);
    auto toTransform = [transform]() {
        switch (transform) {
            case WL_OUTPUT_TRANSFORM_90:
                return OutputDeviceInterface::Transform::Rotated90;
            case WL_OUTPUT_TRANSFORM_180:
                return OutputDeviceInterface::Transform::Rotated180;
            case WL_OUTPUT_TRANSFORM_270:
                return OutputDeviceInterface::Transform::Rotated270;
            case WL_OUTPUT_TRANSFORM_FLIPPED:
                return OutputDeviceInterface::Transform::Flipped;
            case WL_OUTPUT_TRANSFORM_FLIPPED_90:
                return OutputDeviceInterface::Transform::Flipped90;
            case WL_OUTPUT_TRANSFORM_FLIPPED_180:
                return OutputDeviceInterface::Transform::Flipped180;
            case WL_OUTPUT_TRANSFORM_FLIPPED_270:
                return OutputDeviceInterface::Transform::Flipped270;
            case WL_OUTPUT_TRANSFORM_NORMAL:
            default:
                return OutputDeviceInterface::Transform::Normal;
        }
    };
    auto _transform = toTransform();
    OutputDeviceInterface *o = OutputDeviceInterface::get(outputdevice);
    auto s = cast<Private>(resource);
    Q_ASSERT(s);
    s->pendingChanges(o)->d_func()->transform = _transform;
}

void OutputConfigurationInterface::Private::positionCallback(wl_client *client, wl_resource *resource, wl_resource * outputdevice, int32_t x, int32_t y)
{
    Q_UNUSED(client);
    auto _pos = QPoint(x, y);
    OutputDeviceInterface *o = OutputDeviceInterface::get(outputdevice);
    auto s = cast<Private>(resource);
    Q_ASSERT(s);
    s->pendingChanges(o)->d_func()->position = _pos;
}

void OutputConfigurationInterface::Private::scaleCallback(wl_client *client, wl_resource *resource, wl_resource * outputdevice, int32_t scale)
{
    Q_UNUSED(client);
    if (scale <= 0) {
        qCWarning(KWAYLAND_SERVER) << "Requested to scale output device to" << scale << ", but I can't do that.";
        return;
    }
    OutputDeviceInterface *o = OutputDeviceInterface::get(outputdevice);
    auto s = cast<Private>(resource);
    Q_ASSERT(s);
    s->pendingChanges(o)->d_func()->scale = scale;
}

void OutputConfigurationInterface::Private::scaleFCallback(wl_client *client, wl_resource *resource, wl_resource * outputdevice, wl_fixed_t scale_fixed)
{
    Q_UNUSED(client);
    const qreal scale = wl_fixed_to_double(scale_fixed);

    if (scale <= 0) {
        qCWarning(KWAYLAND_SERVER) << "Requested to scale output device to" << scale << ", but I can't do that.";
        return;
    }
    OutputDeviceInterface *o = OutputDeviceInterface::get(outputdevice);
    auto s = cast<Private>(resource);
    Q_ASSERT(s);

    s->pendingChanges(o)->d_func()->scale = scale;
}

void OutputConfigurationInterface::Private::applyCallback(wl_client *client, wl_resource *resource)
{
    Q_UNUSED(client);
    auto s = cast<Private>(resource);
    Q_ASSERT(s);
    s->emitConfigurationChangeRequested();
}

void OutputConfigurationInterface::Private::emitConfigurationChangeRequested() const
{
    auto configinterface = reinterpret_cast<OutputConfigurationInterface *>(q);
    emit outputManagement->configurationChangeRequested(configinterface);
}


OutputConfigurationInterface::Private::Private(OutputConfigurationInterface *q, OutputManagementInterface *c, wl_resource *parentResource)
: Resource::Private(q, c, parentResource, &org_kde_kwin_outputconfiguration_interface, &s_interface)
{
}

OutputConfigurationInterface::Private::~Private() = default;

OutputConfigurationInterface::Private *OutputConfigurationInterface::d_func() const
{
    return reinterpret_cast<Private*>(d.data());
}

QHash<OutputDeviceInterface*, OutputChangeSet*> OutputConfigurationInterface::changes() const
{
    Q_D();
    return d->changes;
}

void OutputConfigurationInterface::setApplied()
{
    Q_D();
    d->clearPendingChanges();
    d->sendApplied();
}

void OutputConfigurationInterface::Private::sendApplied()
{
    org_kde_kwin_outputconfiguration_send_applied(resource);
}

void OutputConfigurationInterface::setFailed()
{
    Q_D();
    d->clearPendingChanges();
    d->sendFailed();
}

void OutputConfigurationInterface::Private::sendFailed()
{
    org_kde_kwin_outputconfiguration_send_failed(resource);
}

OutputChangeSet* OutputConfigurationInterface::Private::pendingChanges(OutputDeviceInterface *outputdevice)
{
    if (!changes.keys().contains(outputdevice)) {
        changes[outputdevice] = new OutputChangeSet(outputdevice, q);
    }
    return changes[outputdevice];
}

bool OutputConfigurationInterface::Private::hasPendingChanges(OutputDeviceInterface *outputdevice) const
{
    if (!changes.keys().contains(outputdevice)) {
        return false;
    }
    auto c = changes[outputdevice];
    return c->enabledChanged() ||
    c->modeChanged() ||
    c->transformChanged() ||
    c->positionChanged() ||
    c->scaleChanged();
}

void OutputConfigurationInterface::Private::clearPendingChanges()
{
    qDeleteAll(changes.begin(), changes.end());
    changes.clear();
}


}
}
