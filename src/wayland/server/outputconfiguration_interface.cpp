/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "display.h"
#include "outputconfiguration_interface.h"
#include "outputdevice_interface.h"
#include "logging.h"
#include "outputchangeset_p.h"

#include <wayland-server.h>
#include "qwayland-server-outputmanagement.h"
#include "qwayland-server-org-kde-kwin-outputdevice.h"

#include <QSize>

namespace KWaylandServer
{

class OutputConfigurationInterfacePrivate : public QtWaylandServer::org_kde_kwin_outputconfiguration
{
public:
    OutputConfigurationInterfacePrivate(OutputConfigurationInterface *q, OutputManagementInterface *outputManagement, wl_resource *resource);

    void sendApplied();
    void sendFailed();
    void emitConfigurationChangeRequested() const;
    void clearPendingChanges();

    bool hasPendingChanges(OutputDeviceInterface *outputdevice) const;
    OutputChangeSet* pendingChanges(OutputDeviceInterface *outputdevice);

    OutputManagementInterface *outputManagement;
    QHash<OutputDeviceInterface*, OutputChangeSet*> changes;
    OutputConfigurationInterface *q;

protected:
    void org_kde_kwin_outputconfiguration_enable(Resource *resource, wl_resource *outputdevice, int32_t enable) override;
    void org_kde_kwin_outputconfiguration_mode(Resource *resource, wl_resource *outputdevice, int32_t mode_id) override;
    void org_kde_kwin_outputconfiguration_transform(Resource *resource, wl_resource *outputdevice, int32_t transform) override;
    void org_kde_kwin_outputconfiguration_position(Resource *resource, wl_resource *outputdevice, int32_t x, int32_t y) override;
    void org_kde_kwin_outputconfiguration_scale(Resource *resource, wl_resource *outputdevice, int32_t scale) override;
    void org_kde_kwin_outputconfiguration_apply(Resource *resource) override;
    void org_kde_kwin_outputconfiguration_scalef(Resource *resource, wl_resource *outputdevice, wl_fixed_t scale) override;
    void org_kde_kwin_outputconfiguration_colorcurves(Resource *resource, wl_resource *outputdevice, wl_array *red, wl_array *green, wl_array *blue) override;
    void org_kde_kwin_outputconfiguration_destroy(Resource *resource) override;
    void org_kde_kwin_outputconfiguration_destroy_resource(Resource *resource) override;
    void org_kde_kwin_outputconfiguration_overscan(Resource *resource, wl_resource *outputdevice, uint32_t overscan) override;
};

void OutputConfigurationInterfacePrivate::org_kde_kwin_outputconfiguration_enable(Resource *resource, wl_resource *outputdevice, int32_t enable)
{
    Q_UNUSED(resource)
    auto _enable = (enable == ORG_KDE_KWIN_OUTPUTDEVICE_ENABLEMENT_ENABLED) ?
                                    OutputDeviceInterface::Enablement::Enabled :
                                    OutputDeviceInterface::Enablement::Disabled;
    
    OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice);
    pendingChanges(output)->d->enabled = _enable;
}

void OutputConfigurationInterfacePrivate::org_kde_kwin_outputconfiguration_mode(Resource *resource, wl_resource *outputdevice, int32_t mode_id)
{
    Q_UNUSED(resource)
    OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice);

    bool modeValid = false;
    for (const auto &m: output->modes()) {
        if (m.id == mode_id) {
            modeValid = true;
            break;
        }
    }
    if (!modeValid) {
        qCWarning(KWAYLAND_SERVER) << "Set invalid mode id:" << mode_id;
        return;
    }
    
    pendingChanges(output)->d->modeId = mode_id;
}

void OutputConfigurationInterfacePrivate::org_kde_kwin_outputconfiguration_transform(Resource *resource, wl_resource *outputdevice, int32_t transform)
{
    Q_UNUSED(resource)
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
    OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice);
    pendingChanges(output)->d->transform = _transform;
}

void OutputConfigurationInterfacePrivate::org_kde_kwin_outputconfiguration_position(Resource *resource, wl_resource *outputdevice, int32_t x, int32_t y)
{
    Q_UNUSED(resource)
    auto _pos = QPoint(x, y);
    OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice);
    pendingChanges(output)->d->position = _pos;
}

void OutputConfigurationInterfacePrivate::org_kde_kwin_outputconfiguration_scale(Resource *resource, wl_resource *outputdevice, int32_t scale)
{
    Q_UNUSED(resource)
    if (scale <= 0) {
        qCWarning(KWAYLAND_SERVER) << "Requested to scale output device to" << scale << ", but I can't do that.";
        return;
    }
    OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice);
    
    pendingChanges(output)->d->scale = scale;
}

void OutputConfigurationInterfacePrivate::org_kde_kwin_outputconfiguration_apply(Resource *resource)
{
    Q_UNUSED(resource)
    emitConfigurationChangeRequested();
}

void OutputConfigurationInterfacePrivate::org_kde_kwin_outputconfiguration_scalef(Resource *resource, wl_resource *outputdevice, wl_fixed_t scale)
{
    Q_UNUSED(resource)
    const qreal doubleScale = wl_fixed_to_double(scale);

    if (doubleScale <= 0) {
        qCWarning(KWAYLAND_SERVER) << "Requested to scale output device to" << doubleScale << ", but I can't do that.";
        return;
    }
    OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice);
    
    pendingChanges(output)->d->scale = doubleScale;
}

void OutputConfigurationInterfacePrivate::org_kde_kwin_outputconfiguration_colorcurves(Resource *resource, wl_resource *outputdevice, wl_array *red, wl_array *green, wl_array *blue)
{
    Q_UNUSED(resource)
    OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice);
    OutputDeviceInterface::ColorCurves oldCc = output->colorCurves();

    auto checkArg = [](const wl_array *newColor, const QVector<quint16> &oldColor) {
        return (newColor->size % sizeof(uint16_t) == 0) &&
                (newColor->size / sizeof(uint16_t) == static_cast<size_t>(oldColor.size()));
    };
    if (!checkArg(red, oldCc.red) || !checkArg(green, oldCc.green) || !checkArg(blue, oldCc.blue)) {
        qCWarning(KWAYLAND_SERVER) << "Requested to change color curves, but have wrong size.";
        return;
    }
    
    OutputDeviceInterface::ColorCurves cc;

    auto fillVector = [](const wl_array *array, QVector<quint16> *v) {
        const uint16_t *pos = (uint16_t*)array->data;

        while((char*)pos < (char*)array->data + array->size) {
            v->append(*pos);
            pos++;
        }
    };
    
    fillVector(red, &cc.red);
    fillVector(green, &cc.green);
    fillVector(blue, &cc.blue);

    pendingChanges(output)->d->colorCurves = cc;
}

void OutputConfigurationInterfacePrivate::org_kde_kwin_outputconfiguration_overscan(Resource *resource, wl_resource *outputdevice, uint32_t overscan)
{
    Q_UNUSED(resource)
    if (overscan > 100) {
        qCWarning(KWAYLAND_SERVER) << "Invalid overscan requested:" << overscan;
        return;
    }
    OutputDeviceInterface *output = OutputDeviceInterface::get(outputdevice);
    pendingChanges(output)->d->overscan = overscan;
}

void OutputConfigurationInterfacePrivate::org_kde_kwin_outputconfiguration_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void OutputConfigurationInterfacePrivate::org_kde_kwin_outputconfiguration_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete q;
}

void OutputConfigurationInterfacePrivate::emitConfigurationChangeRequested() const
{
    auto configinterface = reinterpret_cast<OutputConfigurationInterface *>(q);
    Q_EMIT outputManagement->configurationChangeRequested(configinterface);
}

OutputConfigurationInterfacePrivate::OutputConfigurationInterfacePrivate(OutputConfigurationInterface *q, OutputManagementInterface *outputManagement, wl_resource *resource)
    : QtWaylandServer::org_kde_kwin_outputconfiguration(resource)
    , outputManagement(outputManagement)
    , q(q)
{
}

QHash<OutputDeviceInterface*, OutputChangeSet*> OutputConfigurationInterface::changes() const
{
    return d->changes;
}

void OutputConfigurationInterface::setApplied()
{
    d->clearPendingChanges();
    d->sendApplied();
}

void OutputConfigurationInterfacePrivate::sendApplied()
{
    send_applied();
}

void OutputConfigurationInterface::setFailed()
{
    d->clearPendingChanges();
    d->sendFailed();
}

void OutputConfigurationInterfacePrivate::sendFailed()
{
    send_failed();
}

OutputChangeSet* OutputConfigurationInterfacePrivate::pendingChanges(OutputDeviceInterface *outputdevice)
{
    auto &change = changes[outputdevice];
    if (!change) {
        change = new OutputChangeSet(outputdevice, q);
    }
    return change;
}

bool OutputConfigurationInterfacePrivate::hasPendingChanges(OutputDeviceInterface *outputdevice) const
{
    auto it = changes.constFind(outputdevice);
    if (it == changes.constEnd()) {
        return false;
    }
    auto c = *it;
    return c->enabledChanged() ||
    c->modeChanged() ||
    c->transformChanged() ||
    c->positionChanged() ||
    c->scaleChanged();
}

void OutputConfigurationInterfacePrivate::clearPendingChanges()
{
    qDeleteAll(changes.begin(), changes.end());
    changes.clear();
}

OutputConfigurationInterface::OutputConfigurationInterface(OutputManagementInterface *parent, wl_resource *resource)
    : QObject()
    , d(new OutputConfigurationInterfacePrivate(this, parent, resource))
{
}

OutputConfigurationInterface::~OutputConfigurationInterface()
{
    d->clearPendingChanges();
}

}
