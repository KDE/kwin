/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "outputmanagement_v2_interface.h"
#include "display.h"
#include "outputchangeset_v2.h"
#include "outputchangeset_v2_p.h"
#include "outputdevice_v2_interface.h"
#include "outputmanagement_v2_interface.h"
#include "utils/common.h"

#include "main.h"
#include "outputconfiguration.h"
#include "platform.h"
#include "screens.h"
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
    OutputManagementV2InterfacePrivate(OutputManagementV2Interface *_q, Display *display);

private:
    OutputManagementV2Interface *q;

protected:
    void kde_output_management_v2_create_configuration(Resource *resource, uint32_t id) override;
};

class OutputConfigurationV2Interface : public QtWaylandServer::kde_output_configuration_v2
{
public:
    explicit OutputConfigurationV2Interface(wl_resource *resource);
    ~OutputConfigurationV2Interface() override;

    OutputChangeSetV2 *pendingChanges(OutputDeviceV2Interface *outputdevice);

    bool applied = false;
    QHash<OutputDeviceV2Interface *, OutputChangeSetV2 *> changes;
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

OutputManagementV2InterfacePrivate::OutputManagementV2InterfacePrivate(OutputManagementV2Interface *_q, Display *display)
    : QtWaylandServer::kde_output_management_v2(*display, s_version)
    , q(_q)
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
    , d(new OutputManagementV2InterfacePrivate(this, display))
{
}

OutputManagementV2Interface::~OutputManagementV2Interface() = default;

OutputConfigurationV2Interface::OutputConfigurationV2Interface(wl_resource *resource)
    : QtWaylandServer::kde_output_configuration_v2(resource)
{
}

OutputConfigurationV2Interface::~OutputConfigurationV2Interface()
{
    qDeleteAll(changes.begin(), changes.end());
}

OutputChangeSetV2 *OutputConfigurationV2Interface::pendingChanges(OutputDeviceV2Interface *outputdevice)
{
    auto &change = changes[outputdevice];
    if (!change) {
        change = new OutputChangeSetV2(outputdevice);
    }
    return change;
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_enable(Resource *resource, wl_resource *outputdevice, int32_t enable)
{
    Q_UNUSED(resource)

    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    pendingChanges(output)->d->enabled = enable == 1;
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_mode(Resource *resource, wl_resource *outputdevice, wl_resource *modeResource)
{
    Q_UNUSED(resource)
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    OutputDeviceModeV2Interface *mode = OutputDeviceModeV2Interface::get(modeResource);
    if (!mode) {
        return;
    }

    pendingChanges(output)->d->size = mode->size();
    pendingChanges(output)->d->refreshRate = mode->refreshRate();
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_transform(Resource *resource, wl_resource *outputdevice, int32_t transform)
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

void OutputConfigurationV2Interface::kde_output_configuration_v2_position(Resource *resource, wl_resource *outputdevice, int32_t x, int32_t y)
{
    Q_UNUSED(resource)
    auto _pos = QPoint(x, y);
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    pendingChanges(output)->d->position = _pos;
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

    pendingChanges(output)->d->scale = doubleScale;
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_overscan(Resource *resource, wl_resource *outputdevice, uint32_t overscan)
{
    Q_UNUSED(resource)
    if (overscan > 100) {
        qCWarning(KWIN_CORE) << "Invalid overscan requested:" << overscan;
        return;
    }
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    pendingChanges(output)->d->overscan = overscan;
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_vrr_policy(Resource *resource, wl_resource *outputdevice, uint32_t policy)
{
    Q_UNUSED(resource)
    if (policy > static_cast<uint32_t>(OutputDeviceV2Interface::VrrPolicy::Automatic)) {
        qCWarning(KWIN_CORE) << "Invalid Vrr Policy requested:" << policy;
        return;
    }
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    pendingChanges(output)->d->vrrPolicy = static_cast<OutputDeviceV2Interface::VrrPolicy>(policy);
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_rgb_range(Resource *resource, wl_resource *outputdevice, uint32_t rgbRange)
{
    Q_UNUSED(resource)
    if (rgbRange > static_cast<uint32_t>(OutputDeviceV2Interface::RgbRange::Limited)) {
        qCWarning(KWIN_CORE) << "Invalid Rgb Range requested:" << rgbRange;
        return;
    }
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    pendingChanges(output)->d->rgbRange = static_cast<OutputDeviceV2Interface::RgbRange>(rgbRange);
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

    OutputConfiguration cfg;
    for (auto it = changes.constBegin(); it != changes.constEnd(); ++it) {
        const KWaylandServer::OutputChangeSetV2 *changeset = it.value();
        auto output = kwinApp()->platform()->findOutput(it.key()->uuid());
        if (!output) {
            qCWarning(KWIN_CORE) << "Could NOT find output matching " << it.key()->uuid();
            continue;
        }

        const auto modes = output->modes();
        const auto modeIt = std::find_if(modes.begin(), modes.end(), [&changeset](const auto &mode) {
            return mode->size() == changeset->size() && mode->refreshRate() == changeset->refreshRate();
        });
        if (modeIt == modes.end()) {
            qCWarning(KWIN_CORE).nospace() << "Could not find mode " << changeset->size() << "@" << changeset->refreshRate() << " for output " << this;
            send_failed();
            return;
        }

        auto props = cfg.changeSet(output);
        props->enabled = changeset->enabled();
        props->pos = changeset->position();
        props->scale = changeset->scale();
        props->mode = *modeIt;
        props->transform = static_cast<Output::Transform>(changeset->transform());
        props->overscan = changeset->overscan();
        props->rgbRange = static_cast<Output::RgbRange>(changeset->rgbRange());
        props->vrrPolicy = static_cast<RenderLoop::VrrPolicy>(changeset->vrrPolicy());
    }

    const auto allOutputs = kwinApp()->platform()->outputs();
    bool allDisabled = !std::any_of(allOutputs.begin(), allOutputs.end(), [&cfg](const auto &output) {
        return cfg.changeSet(output)->enabled;
    });
    if (allDisabled) {
        qCWarning(KWIN_CORE) << "Disabling all outputs through configuration changes is not allowed";
        send_failed();
        return;
    }

    if (kwinApp()->platform()->applyOutputChanges(cfg)) {
        if (primaryOutput.has_value() || !kwinApp()->platform()->primaryOutput()->isEnabled()) {
            auto requestedPrimaryOutput = kwinApp()->platform()->findOutput((*primaryOutput)->uuid());
            if (requestedPrimaryOutput && requestedPrimaryOutput->isEnabled()) {
                kwinApp()->platform()->setPrimaryOutput(requestedPrimaryOutput);
            } else {
                Output *defaultPrimaryOutput = nullptr;
                const auto candidates = kwinApp()->platform()->outputs();
                for (Output *output : candidates) {
                    if (output->isEnabled()) {
                        defaultPrimaryOutput = output;
                        break;
                    }
                }
                qCWarning(KWIN_CORE) << "Requested invalid primary screen, using" << defaultPrimaryOutput;
                kwinApp()->platform()->setPrimaryOutput(defaultPrimaryOutput);
            }
        }
        Q_EMIT workspace()->screens()->changed();
        send_applied();
    } else {
        qCDebug(KWIN_CORE) << "Applying config failed";
        send_failed();
    }
}

}
