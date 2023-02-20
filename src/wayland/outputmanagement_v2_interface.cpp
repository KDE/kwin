/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "outputmanagement_v2_interface.h"
#include "display.h"
#include "outputdevice_v2_interface.h"
#include "outputmanagement_v2_interface.h"
#include "utils/common.h"

#include "core/outputbackend.h"
#include "core/outputconfiguration.h"
#include "main.h"
#include "workspace.h"

#include "qwayland-server-kde-output-management-v2.h"

#include <optional>

using namespace KWin;

namespace KWaylandServer
{

static const quint32 s_version = 3;

class OutputManagementV2InterfacePrivate : public QtWaylandServer::kde_output_management_v2
{
public:
    OutputManagementV2InterfacePrivate(Display *display);

protected:
    void kde_output_management_v2_create_configuration(Resource *resource, uint32_t id) override;
};

class OutputConfigurationV2Interface : public QObject, QtWaylandServer::kde_output_configuration_v2
{
    Q_OBJECT
public:
    explicit OutputConfigurationV2Interface(wl_resource *resource);

    bool applied = false;
    bool invalid = false;
    OutputConfiguration config;
    QVector<std::pair<uint32_t, OutputDeviceV2Interface *>> outputOrder;

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
    void kde_output_configuration_v2_set_priority(Resource *resource, wl_resource *output, uint32_t priority) override;
};

OutputManagementV2InterfacePrivate::OutputManagementV2InterfacePrivate(Display *display)
    : QtWaylandServer::kde_output_management_v2(*display, s_version)
{
}

void OutputManagementV2InterfacePrivate::kde_output_management_v2_create_configuration(Resource *resource, uint32_t id)
{
    wl_resource *config_resource = wl_resource_create(resource->client(), &kde_output_configuration_v2_interface, resource->version(), id);
    if (!config_resource) {
        wl_client_post_no_memory(resource->client());
        return;
    }
    new OutputConfigurationV2Interface(config_resource);
}

OutputManagementV2Interface::OutputManagementV2Interface(Display *display, QObject *parent)
    : QObject(parent)
    , d(new OutputManagementV2InterfacePrivate(display))
{
}

OutputManagementV2Interface::~OutputManagementV2Interface() = default;

OutputConfigurationV2Interface::OutputConfigurationV2Interface(wl_resource *resource)
    : QtWaylandServer::kde_output_configuration_v2(resource)
{
    const auto reject = [this](Output *output) {
        invalid = true;
    };
    connect(workspace(), &Workspace::outputAdded, this, reject);
    connect(workspace(), &Workspace::outputRemoved, this, reject);
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_enable(Resource *resource, wl_resource *outputdevice, int32_t enable)
{
    if (invalid) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->enabled = enable;
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_mode(Resource *resource, wl_resource *outputdevice, wl_resource *modeResource)
{
    if (invalid) {
        return;
    }
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    OutputDeviceModeV2Interface *mode = OutputDeviceModeV2Interface::get(modeResource);
    if (output && mode) {
        config.changeSet(output->handle())->mode = mode->handle().lock();
    } else {
        invalid = true;
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_transform(Resource *resource, wl_resource *outputdevice, int32_t transform)
{
    if (invalid) {
        return;
    }
    auto toTransform = [transform]() {
        switch (transform) {
        case WL_OUTPUT_TRANSFORM_90:
            return Output::Transform::Rotated90;
        case WL_OUTPUT_TRANSFORM_180:
            return Output::Transform::Rotated180;
        case WL_OUTPUT_TRANSFORM_270:
            return Output::Transform::Rotated270;
        case WL_OUTPUT_TRANSFORM_FLIPPED:
            return Output::Transform::Flipped;
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
            return Output::Transform::Flipped90;
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            return Output::Transform::Flipped180;
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            return Output::Transform::Flipped270;
        case WL_OUTPUT_TRANSFORM_NORMAL:
        default:
            return Output::Transform::Normal;
        }
    };
    auto _transform = toTransform();
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->transform = _transform;
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_position(Resource *resource, wl_resource *outputdevice, int32_t x, int32_t y)
{
    if (invalid) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->pos = QPoint(x, y);
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_scale(Resource *resource, wl_resource *outputdevice, wl_fixed_t scale)
{
    if (invalid) {
        return;
    }
    qreal doubleScale = wl_fixed_to_double(scale);

    // the fractional scaling protocol only speaks in unit of 120ths
    // using the same scale throughout makes that simpler
    // this also eliminates most loss from wl_fixed
    doubleScale = std::round(doubleScale * 120) / 120;

    if (doubleScale <= 0) {
        qCWarning(KWIN_CORE) << "Requested to scale output device to" << doubleScale << ", but I can't do that.";
        return;
    }

    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->scale = doubleScale;
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_overscan(Resource *resource, wl_resource *outputdevice, uint32_t overscan)
{
    if (invalid) {
        return;
    }
    if (overscan > 100) {
        qCWarning(KWIN_CORE) << "Invalid overscan requested:" << overscan;
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->overscan = overscan;
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_vrr_policy(Resource *resource, wl_resource *outputdevice, uint32_t policy)
{
    if (invalid) {
        return;
    }
    if (policy > static_cast<uint32_t>(RenderLoop::VrrPolicy::Automatic)) {
        qCWarning(KWIN_CORE) << "Invalid Vrr Policy requested:" << policy;
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->vrrPolicy = static_cast<RenderLoop::VrrPolicy>(policy);
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_rgb_range(Resource *resource, wl_resource *outputdevice, uint32_t rgbRange)
{
    if (invalid) {
        return;
    }
    if (rgbRange > static_cast<uint32_t>(Output::RgbRange::Limited)) {
        qCWarning(KWIN_CORE) << "Invalid Rgb Range requested:" << rgbRange;
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->rgbRange = static_cast<Output::RgbRange>(rgbRange);
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_primary_output(Resource *resource, struct ::wl_resource *output)
{
    // intentionally ignored
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_priority(Resource *resource, wl_resource *outputResource, uint32_t priority)
{
    if (invalid) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputResource)) {
        outputOrder.push_back(std::make_pair(priority, output));
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_destroy_resource(Resource *resource)
{
    delete this;
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_apply(Resource *resource)
{
    if (applied) {
        wl_resource_post_error(resource->handle, error_already_applied, "an output configuration can be applied only once");
        return;
    }

    applied = true;
    if (invalid) {
        qCWarning(KWIN_CORE) << "Rejecting configuration change because a request output is no longer available";
        send_failed();
        return;
    }

    const auto allOutputs = kwinApp()->outputBackend()->outputs();
    const bool allDisabled = !std::any_of(allOutputs.begin(), allOutputs.end(), [this](const auto &output) {
        return config.constChangeSet(output)->enabled;
    });
    if (allDisabled) {
        qCWarning(KWIN_CORE) << "Disabling all outputs through configuration changes is not allowed";
        send_failed();
        return;
    }

    QVector<Output *> sortedOrder;
    if (!outputOrder.empty()) {
        const int desktopOutputs = std::count_if(allOutputs.begin(), allOutputs.end(), [](Output *output) {
            return !output->isNonDesktop();
        });
        if (outputOrder.size() != desktopOutputs) {
            qWarning(KWIN_CORE) << "Provided output order doesn't contain all outputs!";
            send_failed();
            return;
        }
        outputOrder.erase(std::remove_if(outputOrder.begin(), outputOrder.end(), [this](const auto &pair) {
                              return !config.constChangeSet(pair.second->handle())->enabled;
                          }),
                          outputOrder.end());
        std::sort(outputOrder.begin(), outputOrder.end(), [](const auto &pair1, const auto &pair2) {
            return pair1.first < pair2.first;
        });
        uint32_t i = 1;
        for (const auto &[index, name] : std::as_const(outputOrder)) {
            if (index != i) {
                qCWarning(KWIN_CORE) << "Provided output order is invalid!";
                send_failed();
                return;
            }
            i++;
        }
        sortedOrder.reserve(outputOrder.size());
        std::transform(outputOrder.begin(), outputOrder.end(), std::back_inserter(sortedOrder), [](const auto &pair) {
            return pair.second->handle();
        });
    }
    if (workspace()->applyOutputConfiguration(config, sortedOrder)) {
        send_applied();
    } else {
        qCDebug(KWIN_CORE) << "Applying config failed";
        send_failed();
    }
}

}
#include "outputmanagement_v2_interface.moc"
