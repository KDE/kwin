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

#include "core/outputconfiguration.h"
#include "core/platform.h"
#include "main.h"
#include "workspace.h"

#include "qwayland-server-kde-output-management-v2.h"

#include <optional>

using namespace KWin;

namespace KWaylandServer
{

static const quint32 s_version = 2;

class OutputManagementV2InterfacePrivate : public QtWaylandServer::kde_output_management_v2
{
public:
    OutputManagementV2InterfacePrivate(Display *display);

protected:
    void kde_output_management_v2_create_configuration(Resource *resource, uint32_t id) override;
};

class OutputConfigurationV2Interface : public QtWaylandServer::kde_output_configuration_v2
{
public:
    explicit OutputConfigurationV2Interface(wl_resource *resource);

    bool applied = false;
    OutputConfiguration config;
    std::optional<OutputDeviceV2Interface *> primaryOutput;

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

OutputManagementV2Interface::OutputManagementV2Interface(Display *display)
    : d(new OutputManagementV2InterfacePrivate(display))
{
}

OutputManagementV2Interface::~OutputManagementV2Interface() = default;

OutputConfigurationV2Interface::OutputConfigurationV2Interface(wl_resource *resource)
    : QtWaylandServer::kde_output_configuration_v2(resource)
{
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_enable(Resource *resource, wl_resource *outputdevice, int32_t enable)
{
    Q_UNUSED(resource)

    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    config.changeSet(output->handle())->enabled = enable;
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_mode(Resource *resource, wl_resource *outputdevice, wl_resource *modeResource)
{
    Q_UNUSED(resource)
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    OutputDeviceModeV2Interface *mode = OutputDeviceModeV2Interface::get(modeResource);
    if (!mode) {
        return;
    }

    config.changeSet(output->handle())->mode = mode->handle().lock();
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_transform(Resource *resource, wl_resource *outputdevice, int32_t transform)
{
    Q_UNUSED(resource)
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
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    config.changeSet(output->handle())->transform = _transform;
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_position(Resource *resource, wl_resource *outputdevice, int32_t x, int32_t y)
{
    Q_UNUSED(resource)
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    config.changeSet(output->handle())->pos = QPoint(x, y);
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_scale(Resource *resource, wl_resource *outputdevice, wl_fixed_t scale)
{
    Q_UNUSED(resource)
    const qreal doubleScale = wl_fixed_to_double(scale);

    if (doubleScale <= 0) {
        qCWarning(KWIN_CORE) << "Requested to scale output device to" << doubleScale << ", but I can't do that.";
        return;
    }

    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    config.changeSet(output->handle())->scale = doubleScale;
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_overscan(Resource *resource, wl_resource *outputdevice, uint32_t overscan)
{
    Q_UNUSED(resource)
    if (overscan > 100) {
        qCWarning(KWIN_CORE) << "Invalid overscan requested:" << overscan;
        return;
    }
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    config.changeSet(output->handle())->overscan = overscan;
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_vrr_policy(Resource *resource, wl_resource *outputdevice, uint32_t policy)
{
    Q_UNUSED(resource)
    if (policy > static_cast<uint32_t>(RenderLoop::VrrPolicy::Automatic)) {
        qCWarning(KWIN_CORE) << "Invalid Vrr Policy requested:" << policy;
        return;
    }
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    config.changeSet(output->handle())->vrrPolicy = static_cast<RenderLoop::VrrPolicy>(policy);
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_rgb_range(Resource *resource, wl_resource *outputdevice, uint32_t rgbRange)
{
    Q_UNUSED(resource)
    if (rgbRange > static_cast<uint32_t>(Output::RgbRange::Limited)) {
        qCWarning(KWIN_CORE) << "Invalid Rgb Range requested:" << rgbRange;
        return;
    }
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    config.changeSet(output->handle())->rgbRange = static_cast<Output::RgbRange>(rgbRange);
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_primary_output(Resource *resource, struct ::wl_resource *output)
{
    Q_UNUSED(resource);
    primaryOutput = OutputDeviceV2Interface::get(output);
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_destroy_resource(Resource *resource)
{
    Q_UNUSED(resource)
    delete this;
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_apply(Resource *resource)
{
    if (applied) {
        wl_resource_post_error(resource->handle, error_already_applied, "an output configuration can be applied only once");
        return;
    }

    applied = true;

    const auto allOutputs = kwinApp()->platform()->outputs();
    const bool allDisabled = !std::any_of(allOutputs.begin(), allOutputs.end(), [this](const auto &output) {
        return config.constChangeSet(output)->enabled;
    });
    if (allDisabled) {
        qCWarning(KWIN_CORE) << "Disabling all outputs through configuration changes is not allowed";
        send_failed();
        return;
    }

    if (workspace()->applyOutputConfiguration(config)) {
        if (primaryOutput.has_value()) {
            auto requestedPrimaryOutput = (*primaryOutput)->handle();
            if (requestedPrimaryOutput && requestedPrimaryOutput->isEnabled()) {
                workspace()->setPrimaryOutput(requestedPrimaryOutput);
            }
        }
        send_applied();
    } else {
        qCDebug(KWIN_CORE) << "Applying config failed";
        send_failed();
    }
}

}
