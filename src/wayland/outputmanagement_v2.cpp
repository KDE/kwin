/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "outputmanagement_v2.h"
#include "core/iccprofile.h"
#include "core/outputbackend.h"
#include "core/outputconfiguration.h"
#include "display.h"
#include "main.h"
#include "outputdevice_v2.h"
#include "outputmanagement_v2.h"
#include "utils/common.h"
#include "workspace.h"

#include "qwayland-server-kde-output-management-v2.h"

#include <KLocalizedString>
#include <cmath>
#include <optional>

namespace KWin
{

static const quint32 s_version = 16;

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
    QList<std::pair<uint32_t, OutputDeviceV2Interface *>> outputOrder;
    QString failureReason;

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
    void kde_output_configuration_v2_set_high_dynamic_range(Resource *resource, wl_resource *outputdevice, uint32_t enable_hdr) override;
    void kde_output_configuration_v2_set_sdr_brightness(Resource *resource, wl_resource *outputdevice, uint32_t sdr_brightness) override;
    void kde_output_configuration_v2_set_wide_color_gamut(Resource *resource, wl_resource *outputdevice, uint32_t enable_wcg) override;
    void kde_output_configuration_v2_set_auto_rotate_policy(Resource *resource, wl_resource *outputdevice, uint32_t auto_rotation_policy) override;
    void kde_output_configuration_v2_set_icc_profile_path(Resource *resource, wl_resource *outputdevice, const QString &profile_path) override;
    void kde_output_configuration_v2_set_brightness_overrides(Resource *resource, wl_resource *outputdevice, int32_t max_peak_brightness, int32_t max_average_brightness, int32_t min_brightness) override;
    void kde_output_configuration_v2_set_sdr_gamut_wideness(Resource *resource, wl_resource *outputdevice, uint32_t gamut_wideness) override;
    void kde_output_configuration_v2_set_color_profile_source(Resource *resource, wl_resource *outputdevice, uint32_t source) override;
    void kde_output_configuration_v2_set_brightness(Resource *resource, wl_resource *outputdevice, uint32_t brightness) override;
    void kde_output_configuration_v2_set_color_power_tradeoff(Resource *resource, wl_resource *outputdevice, uint32_t preference) override;
    void kde_output_configuration_v2_set_dimming(Resource *resource, ::wl_resource *outputdevice, uint32_t multiplier) override;
    void kde_output_configuration_v2_set_replication_source(Resource *resource, struct ::wl_resource *outputdevice, const QString &source) override;
    void kde_output_configuration_v2_set_ddc_ci_allowed(Resource *resource, ::wl_resource *outputdevice, uint32_t allow_ddc_ci) override;
    void kde_output_configuration_v2_set_max_bits_per_color(Resource *resource, struct ::wl_resource *outputdevice, uint32_t max_bpc) override;
    void kde_output_configuration_v2_set_edr_policy(Resource *resource, struct ::wl_resource *outputdevice, uint32_t edrPolicy) override;

    void sendFailure(Resource *resource, const QString &reason);
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
        const auto change = config.changeSet(output->handle());
        const auto modePtr = mode->handle().lock();
        if (!modePtr) {
            invalid = true;
            return;
        }
        change->mode = modePtr;
        change->desiredModeSize = modePtr->size();
        change->desiredModeRefreshRate = modePtr->refreshRate();
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
            return OutputTransform::Rotate90;
        case WL_OUTPUT_TRANSFORM_180:
            return OutputTransform::Rotate180;
        case WL_OUTPUT_TRANSFORM_270:
            return OutputTransform::Rotate270;
        case WL_OUTPUT_TRANSFORM_FLIPPED:
            return OutputTransform::FlipX;
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
            return OutputTransform::FlipX90;
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
            return OutputTransform::FlipX180;
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
            return OutputTransform::FlipX270;
        case WL_OUTPUT_TRANSFORM_NORMAL:
        default:
            return OutputTransform::Normal;
        }
    };
    auto _transform = toTransform();
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        const auto changeset = config.changeSet(output->handle());
        changeset->transform = changeset->manualTransform = _transform;
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
    if (policy > static_cast<uint32_t>(VrrPolicy::Automatic)) {
        qCWarning(KWIN_CORE) << "Invalid Vrr Policy requested:" << policy;
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->vrrPolicy = static_cast<VrrPolicy>(policy);
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

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_high_dynamic_range(Resource *resource, wl_resource *outputdevice, uint32_t enable_hdr)
{
    if (invalid) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->highDynamicRange = enable_hdr == 1;
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_sdr_brightness(Resource *resource, wl_resource *outputdevice, uint32_t sdr_brightness)
{
    if (invalid) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->referenceLuminance = sdr_brightness;
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_wide_color_gamut(Resource *resource, wl_resource *outputdevice, uint32_t enable_wcg)
{
    if (invalid) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->wideColorGamut = enable_wcg == 1;
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_auto_rotate_policy(Resource *resource, wl_resource *outputdevice, uint32_t auto_rotation_policy)
{
    if (invalid) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->autoRotationPolicy = static_cast<Output::AutoRotationPolicy>(auto_rotation_policy);
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_icc_profile_path(Resource *resource, wl_resource *outputdevice, const QString &profile_path)
{
    if (invalid) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        const auto set = config.changeSet(output->handle());
        set->iccProfilePath = profile_path;
        if (auto profile = IccProfile::load(profile_path)) {
            set->iccProfile = std::move(profile.value());
        } else {
            failureReason = profile.error();
        }
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_brightness_overrides(Resource *resource, wl_resource *outputdevice, int32_t max_peak_brightness, int32_t max_average_brightness, int32_t min_brightness)
{
    if (invalid) {
        return;
    }
    if (max_peak_brightness != -1 && max_peak_brightness < 50) {
        failureReason = QStringLiteral("Invalid peak brightness override requested");
        return;
    }
    if (max_average_brightness != -1 && max_average_brightness < 50) {
        failureReason = QStringLiteral("Invalid max average brightness override requested");
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->maxPeakBrightnessOverride = max_peak_brightness == -1 ? std::nullopt : std::optional<double>(max_peak_brightness);
        config.changeSet(output->handle())->maxAverageBrightnessOverride = max_average_brightness == -1 ? std::nullopt : std::optional<double>(max_average_brightness);
        config.changeSet(output->handle())->minBrightnessOverride = min_brightness == -1 ? std::nullopt : std::optional<double>(min_brightness / 10'000.0);
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_sdr_gamut_wideness(Resource *resource, wl_resource *outputdevice, uint32_t gamut_wideness)
{
    if (invalid) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->sdrGamutWideness = gamut_wideness / 10'000.0;
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_color_profile_source(Resource *resource, wl_resource *outputdevice, uint32_t source)
{
    if (invalid) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->colorProfileSource = [source]() {
            switch (source) {
            case color_profile_source_sRGB:
                return Output::ColorProfileSource::sRGB;
            case color_profile_source_ICC:
                return Output::ColorProfileSource::ICC;
            case color_profile_source_EDID:
                return Output::ColorProfileSource::EDID;
            };
            Q_UNREACHABLE();
        }();
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_brightness(Resource *resource, wl_resource *outputdevice, uint32_t brightness)
{
    if (invalid) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->brightness = brightness / 10'000.0;
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_color_power_tradeoff(Resource *resource, wl_resource *outputdevice, uint32_t preference)
{
    if (invalid) {
        return;
    }
    const auto tradeoff = [preference]() -> std::optional<Output::ColorPowerTradeoff> {
        switch (preference) {
        case color_power_tradeoff_efficiency:
            return Output::ColorPowerTradeoff::PreferEfficiency;
        case color_power_tradeoff_accuracy:
            return Output::ColorPowerTradeoff::PreferAccuracy;
        }
        return std::nullopt;
    }();
    if (!tradeoff) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->colorPowerTradeoff = *tradeoff;
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_dimming(Resource *resource, ::wl_resource *outputdevice, uint32_t multiplier)
{
    if (invalid) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->dimming = multiplier / 10'000.0;
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_replication_source(Resource *resource, struct ::wl_resource *outputdevice, const QString &source)
{
    if (invalid) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->replicationSource = source;
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_ddc_ci_allowed(Resource *resource, ::wl_resource *outputdevice, uint32_t allow_ddc_ci)
{
    if (invalid) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        const auto changeset = config.changeSet(output->handle());
        changeset->allowDdcCi = allow_ddc_ci;
        if (!allow_ddc_ci) {
            changeset->allowSdrSoftwareBrightness = true;
            changeset->brightnessDevice = nullptr;
        }
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_max_bits_per_color(Resource *resource, ::wl_resource *outputdevice, uint32_t max_bpc)
{
    if (invalid) {
        return;
    }
    if (OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice)) {
        config.changeSet(output->handle())->maxBitsPerColor = max_bpc;
    }
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_set_edr_policy(Resource *resource, struct ::wl_resource *outputdevice, uint32_t edrPolicy)
{
    if (invalid) {
        return;
    }
    OutputDeviceV2Interface *output = OutputDeviceV2Interface::get(outputdevice);
    if (!output) {
        return;
    }
    switch (edrPolicy) {
    case edr_policy_never:
        config.changeSet(output->handle())->edrPolicy = Output::EdrPolicy::Never;
        break;
    case edr_policy_always:
        config.changeSet(output->handle())->edrPolicy = Output::EdrPolicy::Always;
        break;
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

void OutputConfigurationV2Interface::sendFailure(Resource *resource, const QString &reason)
{
    if (resource->version() >= KDE_OUTPUT_CONFIGURATION_V2_FAILURE_REASON_SINCE_VERSION) {
        send_failure_reason(reason);
    }
    send_failed();
}

void OutputConfigurationV2Interface::kde_output_configuration_v2_apply(Resource *resource)
{
    if (applied) {
        wl_resource_post_error(resource->handle, error_already_applied, "an output configuration can be applied only once");
        return;
    }

    applied = true;
    if (invalid) {
        sendFailure(resource, i18n("One of the relevant outputs is no longer available"));
        return;
    }
    if (!failureReason.isEmpty()) {
        sendFailure(resource, failureReason);
        return;
    }

    const auto allOutputs = kwinApp()->outputBackend()->outputs();
    const bool allDisabled = !std::any_of(allOutputs.begin(), allOutputs.end(), [this](Output *output) {
        const auto changeset = config.constChangeSet(output);
        if (changeset && changeset->enabled.has_value()) {
            return *changeset->enabled;
        } else {
            return output->isEnabled();
        }
    });
    if (allDisabled) {
        sendFailure(resource, i18n("Disabling all outputs through configuration changes is not allowed"));
        return;
    }

    std::optional<QList<Output *>> sortedOrder;
    if (!outputOrder.empty()) {
        const int desktopOutputs = std::count_if(allOutputs.begin(), allOutputs.end(), [](Output *output) {
            return !output->isNonDesktop();
        });
        if (outputOrder.size() != desktopOutputs) {
            sendFailure(resource, i18n("The provided output order doesn't contain all outputs"));
            return;
        }
        outputOrder.erase(std::remove_if(outputOrder.begin(), outputOrder.end(), [this](const auto &pair) {
            const auto changeset = config.constChangeSet(pair.second->handle());
            if (changeset && changeset->enabled.has_value()) {
                return !changeset->enabled.value();
            } else {
                return !pair.second->handle()->isEnabled();
            }
        }),
                          outputOrder.end());
        std::sort(outputOrder.begin(), outputOrder.end(), [](const auto &pair1, const auto &pair2) {
            return pair1.first < pair2.first;
        });
        uint32_t i = 1;
        for (const auto &[index, name] : std::as_const(outputOrder)) {
            if (index != i) {
                sendFailure(resource, i18n("The provided output order is invalid"));
                return;
            }
            i++;
        }
        sortedOrder = QList<Output *>();
        sortedOrder->reserve(outputOrder.size());
        std::transform(outputOrder.begin(), outputOrder.end(), std::back_inserter(*sortedOrder), [](const auto &pair) {
            return pair.second->handle();
        });
    }
    switch (workspace()->applyOutputConfiguration(config, sortedOrder)) {
    case OutputConfigurationError::None:
        send_applied();
        break;
    case OutputConfigurationError::Unknown:
    case OutputConfigurationError::TooManyEnabledOutputs:
        // TODO provide a more accurate error reason once the driver actually gives us anything
        sendFailure(resource, i18n("The driver rejected the output configuration"));
        break;
    }
}

}
#include "outputmanagement_v2.moc"

#include "moc_outputmanagement_v2.cpp"
