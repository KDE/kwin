/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "display.h"
#include "outputconfiguration_v2_interface.h"
#include "outputdevice_v2_interface.h"
#include "logging.h"
#include "outputchangeset_v2_p.h"

#include "qwayland-server-kde-output-management-v2.h"
#include "qwayland-server-kde-output-device-v2.h"

#include <wayland-client-protocol.h>

#include <optional>

namespace KWaylandServer
{
class OutputConfigurationV2InterfacePrivate : public QtWaylandServer::kde_output_configuration_v2
{
public:
    OutputConfigurationV2InterfacePrivate(OutputConfigurationV2Interface *q, OutputManagementV2Interface *outputManagement, wl_resource *resource);

    void sendApplied();
    void sendFailed();
    void emitConfigurationChangeRequested() const;
    void clearPendingChanges();

    bool hasPendingChanges(OutputDeviceV2Interface *outputdevice) const;
    OutputChangeSetV2 *pendingChanges(OutputDeviceV2Interface *outputdevice);

    OutputManagementV2Interface *outputManagement;
    QHash<OutputDeviceV2Interface *, OutputChangeSetV2 *> changes;
    std::optional<OutputDeviceV2Interface *> primaryOutput;
    OutputConfigurationV2Interface *q;

protected:
    void kde_output_configuration_v2_enable(Resource *resource, wl_resource *outputdevice, int32_t enable) override;
    void kde_output_configuration_v2_mode(Resource *resource, struct ::wl_resource *outputdevice, struct ::wl_resource *mode) override;
    void kde_output_configuration_v2_transform(Resource *resource, wl_resource *outputdevice, int32_t transform) override;
    void kde_output_configuration_v2_position(Resource *resource, wl_resource *outputdevice, int32_t x, int32_t y) override;
    void kde_output_configuration_v2_scale(Resource *resource, wl_resource *outputdevice, wl_fixed_t scale) override;
    void kde_output_configuration_v2_apply(Resource *resource) override;
    void kde_output_configuration_v2_destroy(Resource *resource) override;
    void kde_output_configuration_v2_destroy_resource(Resource *resource) override;
    void kde_output_configuration_v2_overscan(Resource *resource, wl_resource *outputdevice, uint32_t overscan) override;
    void kde_output_configuration_v2_set_vrr_policy(Resource *resource, struct ::wl_resource *outputdevice, uint32_t policy) override;
    void kde_output_configuration_v2_set_rgb_range(Resource *resource, wl_resource *outputdevice, uint32_t rgbRange) override;
    void kde_output_configuration_v2_set_primary_output(Resource *resource, struct ::wl_resource *output) override;
};

void OutputConfigurationV2InterfacePrivate::kde_output_configuration_v2_enable(Resource *resource, wl_resource *outputdevice, int32_t enable)
{
    Q_UNUSED(resource)
    
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    pendingChanges(output)->d->enabled = enable == 1;
}

void OutputConfigurationV2InterfacePrivate::kde_output_configuration_v2_mode(Resource *resource, wl_resource *outputdevice, wl_resource *modeResource)
{
    Q_UNUSED(resource)
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    OutputDeviceModeV2Interface *mode = OutputDeviceModeV2Interface::get(modeResource);

    pendingChanges(output)->d->size = mode->size();
    pendingChanges(output)->d->refreshRate = mode->refreshRate();
}

void OutputConfigurationV2InterfacePrivate::kde_output_configuration_v2_transform(Resource *resource, wl_resource *outputdevice, int32_t transform)
{
    Q_UNUSED(resource)
    auto toTransform = [transform]() {
        switch (transform) {
            case WL_OUTPUT_TRANSFORM_90:
                return OutputDeviceV2Interface::Transform::Rotated90;
            case WL_OUTPUT_TRANSFORM_180:
                return OutputDeviceV2Interface::Transform::Rotated180;
            case WL_OUTPUT_TRANSFORM_270:
                return OutputDeviceV2Interface::Transform::Rotated270;
            case WL_OUTPUT_TRANSFORM_FLIPPED:
                return OutputDeviceV2Interface::Transform::Flipped;
            case WL_OUTPUT_TRANSFORM_FLIPPED_90:
                return OutputDeviceV2Interface::Transform::Flipped90;
            case WL_OUTPUT_TRANSFORM_FLIPPED_180:
                return OutputDeviceV2Interface::Transform::Flipped180;
            case WL_OUTPUT_TRANSFORM_FLIPPED_270:
                return OutputDeviceV2Interface::Transform::Flipped270;
            case WL_OUTPUT_TRANSFORM_NORMAL:
            default:
                return OutputDeviceV2Interface::Transform::Normal;
        }
    };
    auto _transform = toTransform();
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    pendingChanges(output)->d->transform = _transform;
}

void OutputConfigurationV2InterfacePrivate::kde_output_configuration_v2_position(Resource *resource, wl_resource *outputdevice, int32_t x, int32_t y)
{
    Q_UNUSED(resource)
    auto _pos = QPoint(x, y);
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    pendingChanges(output)->d->position = _pos;
}

void OutputConfigurationV2InterfacePrivate::kde_output_configuration_v2_scale(Resource *resource, wl_resource *outputdevice, wl_fixed_t scale)
{
    Q_UNUSED(resource)
    const qreal doubleScale = wl_fixed_to_double(scale);

    if (doubleScale <= 0) {
        qCWarning(KWAYLAND_SERVER) << "Requested to scale output device to" << doubleScale << ", but I can't do that.";
        return;
    }
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    
    pendingChanges(output)->d->scale = doubleScale;
}

void OutputConfigurationV2InterfacePrivate::kde_output_configuration_v2_apply(Resource *resource)
{
    Q_UNUSED(resource)
    emitConfigurationChangeRequested();
}

void OutputConfigurationV2InterfacePrivate::kde_output_configuration_v2_overscan(Resource *resource, wl_resource *outputdevice, uint32_t overscan)
{
    Q_UNUSED(resource)
    if (overscan > 100) {
        qCWarning(KWAYLAND_SERVER) << "Invalid overscan requested:" << overscan;
        return;
    }
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    pendingChanges(output)->d->overscan = overscan;
}

void OutputConfigurationV2InterfacePrivate::kde_output_configuration_v2_set_vrr_policy(Resource *resource, wl_resource *outputdevice, uint32_t policy)
{
    Q_UNUSED(resource)
    if (policy > static_cast<uint32_t>(OutputDeviceV2Interface::VrrPolicy::Automatic)) {
        qCWarning(KWAYLAND_SERVER) << "Invalid Vrr Policy requested:" << policy;
        return;
    }
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    pendingChanges(output)->d->vrrPolicy = static_cast<OutputDeviceV2Interface::VrrPolicy>(policy);
}

void OutputConfigurationV2InterfacePrivate::kde_output_configuration_v2_set_rgb_range(Resource *resource, wl_resource *outputdevice, uint32_t rgbRange)
{
    Q_UNUSED(resource)
    if (rgbRange > static_cast<uint32_t>(OutputDeviceV2Interface::RgbRange::Limited)) {
        qCWarning(KWAYLAND_SERVER) << "Invalid Rgb Range requested:" << rgbRange;
        return;
    }
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    pendingChanges(output)->d->rgbRange = static_cast<OutputDeviceV2Interface::RgbRange>(rgbRange);
}

void OutputConfigurationV2InterfacePrivate::kde_output_configuration_v2_set_primary_output(Resource *resource, struct ::wl_resource *output)
{
    Q_UNUSED(resource);
    primaryOutput = OutputDeviceV2Interface::get(output);
}

void OutputConfigurationV2InterfacePrivate::kde_output_configuration_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void OutputConfigurationV2InterfacePrivate::kde_output_configuration_v2_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

void OutputConfigurationV2InterfacePrivate::emitConfigurationChangeRequested() const
{
    Q_EMIT outputManagement->configurationChangeRequested(q);
}

OutputConfigurationV2InterfacePrivate::OutputConfigurationV2InterfacePrivate(OutputConfigurationV2Interface *q, OutputManagementV2Interface *outputManagement, wl_resource *resource)
    : QtWaylandServer::kde_output_configuration_v2(resource)
    , outputManagement(outputManagement)
    , q(q)
{
}

QHash<OutputDeviceV2Interface*, OutputChangeSetV2*> OutputConfigurationV2Interface::changes() const
{
    return d->changes;
}

bool OutputConfigurationV2Interface::primaryChanged() const
{
    return d->primaryOutput.has_value();
}

OutputDeviceV2Interface *OutputConfigurationV2Interface::primary() const
{
    Q_ASSERT(d->primaryOutput.has_value());
    return *d->primaryOutput;
}

void OutputConfigurationV2Interface::setApplied()
{
    d->clearPendingChanges();
    d->sendApplied();
}

void OutputConfigurationV2InterfacePrivate::sendApplied()
{
    send_applied();
}

void OutputConfigurationV2Interface::setFailed()
{
    d->clearPendingChanges();
    d->sendFailed();
}

void OutputConfigurationV2InterfacePrivate::sendFailed()
{
    send_failed();
}

OutputChangeSetV2 *OutputConfigurationV2InterfacePrivate::pendingChanges(OutputDeviceV2Interface *outputdevice)
{
    auto &change = changes[outputdevice];
    if (!change) {
        change = new OutputChangeSetV2(outputdevice, q);
    }
    return change;
}

bool OutputConfigurationV2InterfacePrivate::hasPendingChanges(OutputDeviceV2Interface *outputdevice) const
{
    auto it = changes.constFind(outputdevice);
    if (it == changes.constEnd()) {
        return false;
    }
    auto c = *it;
    return c->enabledChanged() ||
    c->sizeChanged() ||
    c->refreshRateChanged() ||
    c->transformChanged() ||
    c->positionChanged() ||
    c->scaleChanged();
}

void OutputConfigurationV2InterfacePrivate::clearPendingChanges()
{
    qDeleteAll(changes.begin(), changes.end());
    changes.clear();
}

OutputConfigurationV2Interface::OutputConfigurationV2Interface(OutputManagementV2Interface *parent, wl_resource *resource)
    : QObject()
    , d(new OutputConfigurationV2InterfacePrivate(this, parent, resource))
{
}

OutputConfigurationV2Interface::~OutputConfigurationV2Interface()
{
    d->clearPendingChanges();
}

}
