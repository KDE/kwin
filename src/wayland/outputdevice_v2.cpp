/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Méven Car <meven.car@enioka.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "outputdevice_v2.h"

#include "display.h"
#include "display_p.h"
#include "utils/common.h"
#include "utils/resource.h"

#include "core/backendoutput.h"

#include <QDebug>
#include <QPointer>
#include <QString>
#include <QTimer>

#include "qwayland-server-kde-output-device-v2.h"

namespace KWin
{

static const quint32 s_version = 18;

static QtWaylandServer::kde_output_device_v2::transform kwinTransformToOutputDeviceTransform(OutputTransform transform)
{
    return static_cast<QtWaylandServer::kde_output_device_v2::transform>(transform.kind());
}

static QtWaylandServer::kde_output_device_v2::subpixel kwinSubPixelToOutputDeviceSubPixel(BackendOutput::SubPixel subPixel)
{
    return static_cast<QtWaylandServer::kde_output_device_v2::subpixel>(subPixel);
}

static uint32_t kwinCapabilitiesToOutputDeviceCapabilities(BackendOutput::Capabilities caps)
{
    uint32_t ret = 0;
    if (caps & BackendOutput::Capability::Overscan) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_overscan;
    }
    if (caps & BackendOutput::Capability::Vrr) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_vrr;
    }
    if (caps & BackendOutput::Capability::RgbRange) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_rgb_range;
    }
    if (caps & BackendOutput::Capability::HighDynamicRange) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_high_dynamic_range;
    }
    if (caps & BackendOutput::Capability::WideColorGamut) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_wide_color_gamut;
    }
    if (caps & BackendOutput::Capability::AutoRotation) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_auto_rotate;
    }
    if (caps & BackendOutput::Capability::IccProfile) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_icc_profile;
    }
    if (caps & BackendOutput::Capability::BrightnessControl) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_brightness;
    }
    if (caps & BackendOutput::Capability::BuiltInColorProfile) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_built_in_color;
    }
    if (caps & BackendOutput::Capability::DdcCi) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_ddc_ci;
    }
    if (caps & BackendOutput::Capability::MaxBitsPerColor) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_max_bits_per_color;
    }
    if (caps & BackendOutput::Capability::Edr) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_edr;
    }
    if (caps & BackendOutput::Capability::SharpnessControl) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_sharpness;
    }
    if (caps & BackendOutput::Capability::CustomModes) {
        ret |= QtWaylandServer::kde_output_device_v2::capability_custom_modes;
    }
    return ret;
}

static QtWaylandServer::kde_output_device_v2::vrr_policy kwinVrrPolicyToOutputDeviceVrrPolicy(VrrPolicy policy)
{
    return static_cast<QtWaylandServer::kde_output_device_v2::vrr_policy>(policy);
}

static QtWaylandServer::kde_output_device_v2::rgb_range kwinRgbRangeToOutputDeviceRgbRange(BackendOutput::RgbRange range)
{
    return static_cast<QtWaylandServer::kde_output_device_v2::rgb_range>(range);
}

static QtWaylandServer::kde_output_device_v2::auto_rotate_policy kwinAutoRotationToOutputDeviceAutoRotation(BackendOutput::AutoRotationPolicy policy)
{
    return static_cast<QtWaylandServer::kde_output_device_v2::auto_rotate_policy>(policy);
}

static QtWaylandServer::kde_output_device_v2::edr_policy kwinEdrPolicyToOutputDevice(BackendOutput::EdrPolicy policy)
{
    return static_cast<QtWaylandServer::kde_output_device_v2::edr_policy>(policy);
}

class OutputDeviceV2InterfacePrivate : public QtWaylandServer::kde_output_device_v2
{
public:
    OutputDeviceV2InterfacePrivate(OutputDeviceV2Interface *q, Display *display, BackendOutput *handle);
    ~OutputDeviceV2InterfacePrivate() override;

    void sendGeometry(Resource *resource);
    wl_resource *sendNewMode(Resource *resource, OutputDeviceModeV2Interface *mode);
    void sendCurrentMode(Resource *resource);
    void sendDone(Resource *resource);
    void sendUuid(Resource *resource);
    void sendEdid(Resource *resource);
    void sendEnabled(Resource *resource);
    void sendScale(Resource *resource);
    void sendEisaId(Resource *resource);
    void sendName(Resource *resource);
    void sendSerialNumber(Resource *resource);
    void sendCapabilities(Resource *resource);
    void sendOverscan(Resource *resource);
    void sendVrrPolicy(Resource *resource);
    void sendRgbRange(Resource *resource);
    void sendHighDynamicRange(Resource *resource);
    void sendSdrBrightness(Resource *resource);
    void sendWideColorGamut(Resource *resource);
    void sendAutoRotationPolicy(Resource *resource);
    void sendIccProfilePath(Resource *resource);
    void sendBrightnessMetadata(Resource *resource);
    void sendBrightnessOverrides(Resource *resource);
    void sendSdrGamutWideness(Resource *resource);
    void sendColorProfileSource(Resource *resource);
    void sendBrightness(Resource *resource);
    void sendColorPowerTradeoff(Resource *resource);
    void sendDimming(Resource *resource);
    void sendReplicationSource(Resource *resource);
    void sendDdcCiAllowed(Resource *resource);
    void sendMaxBpc(Resource *resource);
    void sendEdrPolicy(Resource *resource);
    void sendSharpness(Resource *resource);
    void sendPriority(Resource *resource);

    OutputDeviceV2Interface *q;
    QPointer<Display> m_display;
    BackendOutput *m_handle;
    QSize m_physicalSize;
    QPoint m_globalPosition;
    QString m_manufacturer = QStringLiteral("org.kde.kwin");
    QString m_model = QStringLiteral("none");
    qreal m_scale = 1.0;
    QString m_serialNumber;
    QString m_eisaId;
    QString m_name;
    subpixel m_subPixel = subpixel_unknown;
    transform m_transform = transform_normal;
    std::vector<std::unique_ptr<OutputDeviceModeV2Interface>> m_modes;
    OutputDeviceModeV2Interface *m_currentMode = nullptr;
    QByteArray m_edid;
    bool m_enabled = true;
    QString m_uuid;
    uint32_t m_capabilities = 0;
    uint32_t m_overscan = 0;
    vrr_policy m_vrrPolicy = vrr_policy_automatic;
    rgb_range m_rgbRange = rgb_range_automatic;
    bool m_highDynamicRange = false;
    uint32_t m_referenceLuminance = 200;
    bool m_wideColorGamut = false;
    auto_rotate_policy m_autoRotation = auto_rotate_policy::auto_rotate_policy_in_tablet_mode;
    QString m_iccProfilePath;
    std::optional<double> m_maxPeakBrightness;
    std::optional<double> m_maxAverageBrightness;
    double m_minBrightness = 0;
    double m_sdrGamutWideness = 0;
    std::optional<double> m_maxPeakBrightnessOverride;
    std::optional<double> m_maxAverageBrightnessOverride;
    std::optional<double> m_minBrightnessOverride;
    color_profile_source m_colorProfile = color_profile_source::color_profile_source_sRGB;
    double m_brightness = 1.0;
    color_power_tradeoff m_powerColorTradeoff = color_power_tradeoff_efficiency;
    QTimer m_doneTimer;
    uint32_t m_dimming = 10'000;
    QString m_replicationSource;
    bool m_ddcCiAllowed = true;
    uint32_t m_maxBpc = 0;
    BackendOutput::BpcRange m_maxBpcRange;
    std::optional<uint32_t> m_automaticMaxBitsPerColorLimit;
    BackendOutput::EdrPolicy m_edrPolicy = BackendOutput::EdrPolicy::Always;
    double m_sharpness = 0;
    uint32_t m_priority = 0;

protected:
    void kde_output_device_v2_bind_resource(Resource *resource) override;
    void kde_output_device_v2_destroy_global() override;
};

class OutputDeviceModeV2InterfacePrivate : public QtWaylandServer::kde_output_device_mode_v2
{
public:
    struct ModeResource : Resource
    {
        OutputDeviceV2InterfacePrivate::Resource *output;
    };

    OutputDeviceModeV2InterfacePrivate(OutputDeviceModeV2Interface *q, std::shared_ptr<OutputMode> handle);
    ~OutputDeviceModeV2InterfacePrivate() override;

    Resource *createResource(OutputDeviceV2InterfacePrivate::Resource *output);
    Resource *findResource(OutputDeviceV2InterfacePrivate::Resource *output) const;

    void bindResource(Resource *resource);

    static OutputDeviceModeV2InterfacePrivate *get(OutputDeviceModeV2Interface *mode)
    {
        return mode->d.get();
    }

    OutputDeviceModeV2Interface *q;
    std::weak_ptr<OutputMode> m_handle;
    QSize m_size;
    int m_refreshRate = 60000;
    OutputMode::Flags m_flags;

protected:
    Resource *kde_output_device_mode_v2_allocate() override;
};

OutputDeviceV2InterfacePrivate::OutputDeviceV2InterfacePrivate(OutputDeviceV2Interface *q, Display *display, BackendOutput *handle)
    : QtWaylandServer::kde_output_device_v2(*display, s_version)
    , q(q)
    , m_display(display)
    , m_handle(handle)
{
    DisplayPrivate *displayPrivate = DisplayPrivate::get(display);
    displayPrivate->outputdevicesV2.append(q);
}

OutputDeviceV2InterfacePrivate::~OutputDeviceV2InterfacePrivate()
{
    if (m_display) {
        DisplayPrivate *displayPrivate = DisplayPrivate::get(m_display);
        displayPrivate->outputdevicesV2.removeOne(q);
    }
}

OutputDeviceV2Interface::OutputDeviceV2Interface(Display *display, BackendOutput *handle, QObject *parent)
    : QObject(parent)
    , d(new OutputDeviceV2InterfacePrivate(this, display, handle))
{
    updateEnabled();
    updateManufacturer();
    updateEdid();
    updateUuid();
    updateModel();
    updatePhysicalSize();
    updateGlobalPosition();
    updateScale();
    updateTransform();
    updateEisaId();
    updateSerialNumber();
    updateSubPixel();
    updateOverscan();
    updateCapabilities();
    updateVrrPolicy();
    updateRgbRange();
    updateName();
    updateModes();
    updateHighDynamicRange();
    updateSdrBrightness();
    updateWideColorGamut();
    updateAutoRotate();
    updateIccProfilePath();
    updateBrightnessMetadata();
    updateBrightnessOverrides();
    updateSdrGamutWideness();
    updateColorProfileSource();
    updateBrightness();
    updateColorPowerTradeoff();
    updateDimming();
    updateReplicationSource();
    updateDdcCiAllowed();
    updateMaxBpc();
    updateEdrPolicy();
    updateSharpness();
    updatePriority();

    connect(handle, &BackendOutput::positionChanged,
            this, &OutputDeviceV2Interface::updateGlobalPosition);
    connect(handle, &BackendOutput::scaleSettingChanged,
            this, &OutputDeviceV2Interface::updateScale);
    connect(handle, &BackendOutput::enabledChanged,
            this, &OutputDeviceV2Interface::updateEnabled);
    connect(handle, &BackendOutput::transformChanged,
            this, &OutputDeviceV2Interface::updateTransform);
    connect(handle, &BackendOutput::currentModeChanged,
            this, &OutputDeviceV2Interface::updateCurrentMode);
    connect(handle, &BackendOutput::capabilitiesChanged,
            this, &OutputDeviceV2Interface::updateCapabilities);
    connect(handle, &BackendOutput::overscanChanged,
            this, &OutputDeviceV2Interface::updateOverscan);
    connect(handle, &BackendOutput::vrrPolicyChanged,
            this, &OutputDeviceV2Interface::updateVrrPolicy);
    connect(handle, &BackendOutput::modesChanged,
            this, &OutputDeviceV2Interface::updateModes);
    connect(handle, &BackendOutput::rgbRangeChanged,
            this, &OutputDeviceV2Interface::updateRgbRange);
    connect(handle, &BackendOutput::highDynamicRangeChanged, this, &OutputDeviceV2Interface::updateHighDynamicRange);
    connect(handle, &BackendOutput::referenceLuminanceChanged, this, &OutputDeviceV2Interface::updateSdrBrightness);
    connect(handle, &BackendOutput::wideColorGamutChanged, this, &OutputDeviceV2Interface::updateWideColorGamut);
    connect(handle, &BackendOutput::autoRotationPolicyChanged, this, &OutputDeviceV2Interface::updateAutoRotate);
    connect(handle, &BackendOutput::iccProfileChanged, this, &OutputDeviceV2Interface::updateIccProfilePath);
    connect(handle, &BackendOutput::brightnessMetadataChanged, this, &OutputDeviceV2Interface::updateBrightnessMetadata);
    connect(handle, &BackendOutput::brightnessMetadataChanged, this, &OutputDeviceV2Interface::updateBrightnessOverrides);
    connect(handle, &BackendOutput::sdrGamutWidenessChanged, this, &OutputDeviceV2Interface::updateSdrGamutWideness);
    connect(handle, &BackendOutput::colorProfileSourceChanged, this, &OutputDeviceV2Interface::updateColorProfileSource);
    connect(handle, &BackendOutput::brightnessChanged, this, &OutputDeviceV2Interface::updateBrightness);
    connect(handle, &BackendOutput::colorPowerTradeoffChanged, this, &OutputDeviceV2Interface::updateColorPowerTradeoff);
    connect(handle, &BackendOutput::dimmingChanged, this, &OutputDeviceV2Interface::updateDimming);
    connect(handle, &BackendOutput::uuidChanged, this, &OutputDeviceV2Interface::updateUuid);
    connect(handle, &BackendOutput::replicationSourceChanged, this, &OutputDeviceV2Interface::updateReplicationSource);
    connect(handle, &BackendOutput::allowDdcCiChanged, this, &OutputDeviceV2Interface::updateDdcCiAllowed);
    connect(handle, &BackendOutput::maxBitsPerColorChanged, this, &OutputDeviceV2Interface::updateMaxBpc);
    connect(handle, &BackendOutput::edrPolicyChanged, this, &OutputDeviceV2Interface::updateEdrPolicy);
    connect(handle, &BackendOutput::sharpnessChanged, this, &OutputDeviceV2Interface::updateSharpness);
    connect(handle, &BackendOutput::priorityChanged, this, &OutputDeviceV2Interface::updatePriority);

    // Delay the done event to batch property updates.
    d->m_doneTimer.setSingleShot(true);
    d->m_doneTimer.setInterval(0);
    connect(&d->m_doneTimer, &QTimer::timeout, this, [this]() {
        const auto resources = d->resourceMap();
        for (auto resource : resources) {
            d->sendDone(resource);
        }
    });
}

OutputDeviceV2Interface::~OutputDeviceV2Interface()
{
}

void OutputDeviceV2Interface::remove()
{
    if (d->isGlobalRemoved()) {
        return;
    }

    d->m_doneTimer.stop();

    if (d->m_display) {
        DisplayPrivate *displayPrivate = DisplayPrivate::get(d->m_display);
        displayPrivate->outputdevicesV2.removeOne(this);
    }

    // NOTE that output modes can only be deleted after the global remove,
    // otherwise the client temporarily has an output without any modes
    d->globalRemove();
}

void OutputDeviceV2Interface::scheduleDone()
{
    d->m_doneTimer.start();
}

BackendOutput *OutputDeviceV2Interface::handle() const
{
    return d->m_handle;
}

void OutputDeviceV2InterfacePrivate::kde_output_device_v2_destroy_global()
{
    delete q;
}

void OutputDeviceV2InterfacePrivate::kde_output_device_v2_bind_resource(Resource *resource)
{
    if (isGlobalRemoved()) {
        return;
    }

    sendGeometry(resource);
    sendScale(resource);
    sendEisaId(resource);
    sendName(resource);
    sendSerialNumber(resource);

    for (const auto &mode : m_modes) {
        sendNewMode(resource, mode.get());
    }
    sendCurrentMode(resource);
    sendUuid(resource);
    sendEdid(resource);
    sendEnabled(resource);
    sendCapabilities(resource);
    sendOverscan(resource);
    sendVrrPolicy(resource);
    sendRgbRange(resource);
    sendHighDynamicRange(resource);
    sendSdrBrightness(resource);
    sendWideColorGamut(resource);
    sendAutoRotationPolicy(resource);
    sendIccProfilePath(resource);
    sendBrightnessMetadata(resource);
    sendBrightnessOverrides(resource);
    sendSdrGamutWideness(resource);
    sendColorProfileSource(resource);
    sendBrightness(resource);
    sendColorPowerTradeoff(resource);
    sendDimming(resource);
    sendReplicationSource(resource);
    sendDdcCiAllowed(resource);
    sendMaxBpc(resource);
    sendEdrPolicy(resource);
    sendSharpness(resource);
    sendPriority(resource);
    sendDone(resource);
}

wl_resource *OutputDeviceV2InterfacePrivate::sendNewMode(Resource *resource, OutputDeviceModeV2Interface *mode)
{
    auto privateMode = OutputDeviceModeV2InterfacePrivate::get(mode);
    // bind mode to client
    const auto modeResource = privateMode->createResource(resource);

    send_mode(resource->handle, modeResource->handle);

    privateMode->bindResource(modeResource);

    return modeResource->handle;
}

void OutputDeviceV2InterfacePrivate::sendCurrentMode(Resource *outputResource)
{
    const auto modeResource = OutputDeviceModeV2InterfacePrivate::get(m_currentMode)->findResource(outputResource);
    send_current_mode(outputResource->handle, modeResource->handle);
}

void OutputDeviceV2InterfacePrivate::sendGeometry(Resource *resource)
{
    send_geometry(resource->handle,
                  m_globalPosition.x(),
                  m_globalPosition.y(),
                  m_physicalSize.width(),
                  m_physicalSize.height(),
                  m_subPixel,
                  m_manufacturer,
                  m_model,
                  m_transform);
}

void OutputDeviceV2InterfacePrivate::sendScale(Resource *resource)
{
    send_scale(resource->handle, wl_fixed_from_double(m_scale));
}

void OutputDeviceV2InterfacePrivate::sendSerialNumber(Resource *resource)
{
    send_serial_number(resource->handle, m_serialNumber);
}

void OutputDeviceV2InterfacePrivate::sendEisaId(Resource *resource)
{
    send_eisa_id(resource->handle, m_eisaId);
}

void OutputDeviceV2InterfacePrivate::sendName(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_NAME_SINCE_VERSION) {
        send_name(resource->handle, m_name);
    }
}

void OutputDeviceV2InterfacePrivate::sendDone(Resource *resource)
{
    send_done(resource->handle);
}

void OutputDeviceV2InterfacePrivate::sendEdid(Resource *resource)
{
    send_edid(resource->handle, QString::fromStdString(m_edid.toBase64().toStdString()));
}

void OutputDeviceV2InterfacePrivate::sendEnabled(Resource *resource)
{
    send_enabled(resource->handle, m_enabled);
}

void OutputDeviceV2InterfacePrivate::sendUuid(Resource *resource)
{
    send_uuid(resource->handle, m_uuid);
}

void OutputDeviceV2InterfacePrivate::sendCapabilities(Resource *resource)
{
    send_capabilities(resource->handle, m_capabilities);
}

void OutputDeviceV2InterfacePrivate::sendOverscan(Resource *resource)
{
    send_overscan(resource->handle, m_overscan);
}

void OutputDeviceV2InterfacePrivate::sendVrrPolicy(Resource *resource)
{
    send_vrr_policy(resource->handle, m_vrrPolicy);
}

void OutputDeviceV2InterfacePrivate::sendRgbRange(Resource *resource)
{
    send_rgb_range(resource->handle, m_rgbRange);
}

void OutputDeviceV2InterfacePrivate::sendHighDynamicRange(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_HIGH_DYNAMIC_RANGE_SINCE_VERSION) {
        send_high_dynamic_range(resource->handle, m_highDynamicRange ? 1 : 0);
    }
}

void OutputDeviceV2InterfacePrivate::sendSdrBrightness(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_SDR_BRIGHTNESS_SINCE_VERSION) {
        send_sdr_brightness(resource->handle, m_referenceLuminance);
    }
}

void OutputDeviceV2InterfacePrivate::sendWideColorGamut(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_WIDE_COLOR_GAMUT_SINCE_VERSION) {
        send_wide_color_gamut(resource->handle, m_wideColorGamut ? 1 : 0);
    }
}

void OutputDeviceV2InterfacePrivate::sendAutoRotationPolicy(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_AUTO_ROTATE_POLICY_SINCE_VERSION) {
        send_auto_rotate_policy(resource->handle, m_autoRotation);
    }
}

void OutputDeviceV2InterfacePrivate::sendIccProfilePath(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_ICC_PROFILE_PATH_SINCE_VERSION) {
        send_icc_profile_path(resource->handle, m_iccProfilePath);
    }
}

void OutputDeviceV2InterfacePrivate::sendBrightnessMetadata(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_BRIGHTNESS_METADATA_SINCE_VERSION) {
        send_brightness_metadata(resource->handle, std::round(m_maxPeakBrightness.value_or(0)), std::round(m_maxAverageBrightness.value_or(0)), std::round(m_minBrightness * 10'000));
    }
}

void OutputDeviceV2InterfacePrivate::sendBrightnessOverrides(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_BRIGHTNESS_OVERRIDES_SINCE_VERSION) {
        send_brightness_overrides(resource->handle, std::round(m_maxPeakBrightnessOverride.value_or(-1)), std::round(m_maxAverageBrightnessOverride.value_or(-1)), std::round(m_minBrightnessOverride.value_or(-0.000'1) * 10'000));
    }
}

void OutputDeviceV2InterfacePrivate::sendSdrGamutWideness(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_SDR_GAMUT_WIDENESS_SINCE_VERSION) {
        send_sdr_gamut_wideness(resource->handle, std::clamp<uint32_t>(m_sdrGamutWideness * 10'000, 0, 10'000));
    }
}

void OutputDeviceV2InterfacePrivate::sendColorProfileSource(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_COLOR_PROFILE_SOURCE_SINCE_VERSION) {
        send_color_profile_source(resource->handle, m_colorProfile);
    }
}

void OutputDeviceV2InterfacePrivate::sendBrightness(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_BRIGHTNESS_SINCE_VERSION) {
        send_brightness(resource->handle, m_brightness);
    }
}

void OutputDeviceV2InterfacePrivate::sendColorPowerTradeoff(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_COLOR_POWER_TRADEOFF_SINCE_VERSION) {
        send_color_power_tradeoff(resource->handle, m_powerColorTradeoff);
    }
}

void OutputDeviceV2InterfacePrivate::sendDimming(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_DIMMING_SINCE_VERSION) {
        send_dimming(resource->handle, m_dimming);
    }
}

void OutputDeviceV2InterfacePrivate::sendReplicationSource(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_REPLICATION_SOURCE_SINCE_VERSION) {
        send_replication_source(resource->handle, m_replicationSource);
    }
}

void OutputDeviceV2InterfacePrivate::sendDdcCiAllowed(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_DDC_CI_ALLOWED_SINCE_VERSION) {
        send_ddc_ci_allowed(resource->handle, m_ddcCiAllowed);
    }
}

void OutputDeviceV2InterfacePrivate::sendMaxBpc(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_MAX_BITS_PER_COLOR_SINCE_VERSION) {
        send_max_bits_per_color(resource->handle, m_maxBpc);
        send_automatic_max_bits_per_color_limit(resource->handle, m_automaticMaxBitsPerColorLimit.value_or(0));
        send_max_bits_per_color_range(resource->handle, m_maxBpcRange.min, m_maxBpcRange.max);
    }
}

void OutputDeviceV2InterfacePrivate::sendEdrPolicy(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_EDR_POLICY_SINCE_VERSION) {
        send_edr_policy(resource->handle, kwinEdrPolicyToOutputDevice(m_edrPolicy));
    }
}

void OutputDeviceV2InterfacePrivate::sendSharpness(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_SHARPNESS_SINCE_VERSION) {
        send_sharpness(resource->handle, m_sharpness);
    }
}

void OutputDeviceV2InterfacePrivate::sendPriority(Resource *resource)
{
    if (resource->version() >= KDE_OUTPUT_DEVICE_V2_PRIORITY_SINCE_VERSION) {
        send_priority(resource->handle, m_priority);
    }
}

void OutputDeviceV2Interface::updateGeometry()
{
    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        d->sendGeometry(resource);
    }
    scheduleDone();
}

void OutputDeviceV2Interface::updatePhysicalSize()
{
    d->m_physicalSize = d->m_handle->physicalSize();
}

void OutputDeviceV2Interface::updateGlobalPosition()
{
    const QPoint arg = d->m_handle->position();
    if (d->m_globalPosition == arg) {
        return;
    }
    d->m_globalPosition = arg;
    updateGeometry();
}

void OutputDeviceV2Interface::updateManufacturer()
{
    d->m_manufacturer = d->m_handle->manufacturer();
}

void OutputDeviceV2Interface::updateModel()
{
    d->m_model = d->m_handle->model();
}

void OutputDeviceV2Interface::updateSerialNumber()
{
    d->m_serialNumber = d->m_handle->serialNumber();
}

void OutputDeviceV2Interface::updateEisaId()
{
    d->m_eisaId = d->m_handle->eisaId();
}

void OutputDeviceV2Interface::updateName()
{
    d->m_name = d->m_handle->name();
}

void OutputDeviceV2Interface::updateSubPixel()
{
    const auto arg = kwinSubPixelToOutputDeviceSubPixel(d->m_handle->subPixel());
    if (d->m_subPixel != arg) {
        d->m_subPixel = arg;
        updateGeometry();
    }
}

void OutputDeviceV2Interface::updateTransform()
{
    const auto arg = kwinTransformToOutputDeviceTransform(d->m_handle->transform());
    if (d->m_transform != arg) {
        d->m_transform = arg;
        updateGeometry();
    }
}

void OutputDeviceV2Interface::updateScale()
{
    const qreal scale = d->m_handle->scaleSetting();
    if (qFuzzyCompare(d->m_scale, scale)) {
        return;
    }
    d->m_scale = scale;
    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        d->sendScale(resource);
    }
    scheduleDone();
}

void OutputDeviceV2Interface::updateModes()
{
    auto oldModes = std::move(d->m_modes);
    d->m_currentMode = nullptr;

    const auto clientResources = d->resourceMap();
    const auto nativeModes = d->m_handle->modes();

    for (const std::shared_ptr<OutputMode> &mode : nativeModes) {
        d->m_modes.push_back(std::make_unique<OutputDeviceModeV2Interface>(mode));
        OutputDeviceModeV2Interface *deviceMode = d->m_modes.back().get();

        if (d->m_handle->currentMode() == mode) {
            d->m_currentMode = deviceMode;
        }

        for (auto resource : clientResources) {
            d->sendNewMode(resource, deviceMode);
        }
    }

    for (auto resource : clientResources) {
        d->sendCurrentMode(resource);
    }

    // make sure old modes are removed before the done event
    oldModes.clear();
    scheduleDone();
}

void OutputDeviceV2Interface::updateCurrentMode()
{
    for (const auto &mode : d->m_modes) {
        if (mode->handle().lock() == d->m_handle->currentMode()) {
            if (d->m_currentMode != mode.get()) {
                d->m_currentMode = mode.get();
                const auto clientResources = d->resourceMap();
                for (auto resource : clientResources) {
                    d->sendCurrentMode(resource);
                }
                updateGeometry();
            }
            return;
        }
    }
}

void OutputDeviceV2Interface::updateEdid()
{
    d->m_edid = d->m_handle->edid().raw();
    const auto clientResources = d->resourceMap();
    for (auto resource : clientResources) {
        d->sendEdid(resource);
    }
    scheduleDone();
}

void OutputDeviceV2Interface::updateEnabled()
{
    bool enabled = d->m_handle->isEnabled();
    if (d->m_enabled != enabled) {
        d->m_enabled = enabled;
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendEnabled(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateUuid()
{
    const QString uuid = d->m_handle->uuid();
    if (d->m_uuid != uuid) {
        d->m_uuid = uuid;
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendUuid(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateCapabilities()
{
    const uint32_t cap = kwinCapabilitiesToOutputDeviceCapabilities(d->m_handle->capabilities());
    if (d->m_capabilities != cap) {
        d->m_capabilities = cap;
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendCapabilities(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateOverscan()
{
    const uint32_t overscan = d->m_handle->overscan();
    if (d->m_overscan != overscan) {
        d->m_overscan = overscan;
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendOverscan(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateVrrPolicy()
{
    const auto policy = kwinVrrPolicyToOutputDeviceVrrPolicy(d->m_handle->vrrPolicy());
    if (d->m_vrrPolicy != policy) {
        d->m_vrrPolicy = policy;
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendVrrPolicy(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateRgbRange()
{
    const auto rgbRange = kwinRgbRangeToOutputDeviceRgbRange(d->m_handle->rgbRange());
    if (d->m_rgbRange != rgbRange) {
        d->m_rgbRange = rgbRange;
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendRgbRange(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateHighDynamicRange()
{
    if (d->m_highDynamicRange != d->m_handle->highDynamicRange()) {
        d->m_highDynamicRange = d->m_handle->highDynamicRange();
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendHighDynamicRange(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateSdrBrightness()
{
    if (d->m_referenceLuminance != d->m_handle->referenceLuminance()) {
        d->m_referenceLuminance = d->m_handle->referenceLuminance();
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendSdrBrightness(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateWideColorGamut()
{
    if (d->m_wideColorGamut != d->m_handle->wideColorGamut()) {
        d->m_wideColorGamut = d->m_handle->wideColorGamut();
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendWideColorGamut(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateAutoRotate()
{
    const auto policy = kwinAutoRotationToOutputDeviceAutoRotation(d->m_handle->autoRotationPolicy());
    if (d->m_autoRotation != policy) {
        d->m_autoRotation = policy;
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendAutoRotationPolicy(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateIccProfilePath()
{
    if (d->m_iccProfilePath != d->m_handle->iccProfilePath()) {
        d->m_iccProfilePath = d->m_handle->iccProfilePath();
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendIccProfilePath(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateBrightnessMetadata()
{
    if (d->m_maxPeakBrightness != d->m_handle->maxPeakBrightness() || d->m_maxAverageBrightness != d->m_handle->maxAverageBrightness() || d->m_minBrightness != d->m_handle->minBrightness()) {
        d->m_maxPeakBrightness = d->m_handle->maxPeakBrightness();
        d->m_maxAverageBrightness = d->m_handle->maxAverageBrightness();
        d->m_minBrightness = d->m_handle->minBrightness();
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendBrightnessMetadata(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateBrightnessOverrides()
{
    if (d->m_maxPeakBrightnessOverride != d->m_handle->maxPeakBrightnessOverride() || d->m_maxAverageBrightnessOverride != d->m_handle->maxAverageBrightnessOverride() || d->m_minBrightnessOverride != d->m_handle->minBrightnessOverride()) {
        d->m_maxPeakBrightnessOverride = d->m_handle->maxPeakBrightnessOverride();
        d->m_maxAverageBrightnessOverride = d->m_handle->maxAverageBrightnessOverride();
        d->m_minBrightnessOverride = d->m_handle->minBrightnessOverride();
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendBrightnessOverrides(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateSdrGamutWideness()
{
    if (d->m_sdrGamutWideness != d->m_handle->sdrGamutWideness()) {
        d->m_sdrGamutWideness = d->m_handle->sdrGamutWideness();
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendSdrGamutWideness(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateColorProfileSource()
{
    const auto waylandColorProfileSource = [this]() {
        switch (d->m_handle->colorProfileSource()) {
        case BackendOutput::ColorProfileSource::sRGB:
            return QtWaylandServer::kde_output_device_v2::color_profile_source_sRGB;
        case BackendOutput::ColorProfileSource::ICC:
            return QtWaylandServer::kde_output_device_v2::color_profile_source_ICC;
        case BackendOutput::ColorProfileSource::EDID:
            return QtWaylandServer::kde_output_device_v2::color_profile_source_EDID;
        };
        Q_UNREACHABLE();
    }();
    if (d->m_colorProfile != waylandColorProfileSource) {
        d->m_colorProfile = waylandColorProfileSource;
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendColorProfileSource(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateBrightness()
{
    const uint32_t newBrightness = std::round(d->m_handle->brightnessSetting() * 10'000);
    if (d->m_brightness != newBrightness) {
        d->m_brightness = newBrightness;
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendBrightness(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateColorPowerTradeoff()
{
    const auto colorPowerTradeoff = [this]() {
        switch (d->m_handle->colorPowerTradeoff()) {
        case BackendOutput::ColorPowerTradeoff::PreferEfficiency:
            return QtWaylandServer::kde_output_device_v2::color_power_tradeoff_efficiency;
        case BackendOutput::ColorPowerTradeoff::PreferAccuracy:
            return QtWaylandServer::kde_output_device_v2::color_power_tradeoff_accuracy;
        }
        Q_UNREACHABLE();
    }();
    if (d->m_powerColorTradeoff != colorPowerTradeoff) {
        d->m_powerColorTradeoff = colorPowerTradeoff;
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendColorPowerTradeoff(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateDimming()
{
    const uint32_t newDimming = std::round(d->m_handle->dimming() * 10'000);
    if (d->m_dimming != newDimming) {
        d->m_dimming = newDimming;
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendDimming(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateReplicationSource()
{
    const QString newSource = d->m_handle->replicationSource();
    if (d->m_replicationSource != newSource) {
        d->m_replicationSource = newSource;
        const auto clientResources = d->resourceMap();
        for (auto resource : clientResources) {
            d->sendReplicationSource(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateMaxBpc()
{
    if (d->m_maxBpc != d->m_handle->maxBitsPerColor()
        || d->m_maxBpcRange != d->m_handle->bitsPerColorRange()
        || d->m_automaticMaxBitsPerColorLimit != d->m_handle->automaticMaxBitsPerColorLimit()) {
        d->m_maxBpc = d->m_handle->maxBitsPerColor();
        d->m_maxBpcRange = d->m_handle->bitsPerColorRange();
        d->m_automaticMaxBitsPerColorLimit = d->m_handle->automaticMaxBitsPerColorLimit();
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendMaxBpc(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateDdcCiAllowed()
{
    const bool newDdcCiAllowed = d->m_handle->allowDdcCi();
    if (d->m_ddcCiAllowed != newDdcCiAllowed) {
        d->m_ddcCiAllowed = newDdcCiAllowed;
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendDdcCiAllowed(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateEdrPolicy()
{
    if (d->m_edrPolicy != d->m_handle->edrPolicy()) {
        d->m_edrPolicy = d->m_handle->edrPolicy();
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendEdrPolicy(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updateSharpness()
{
    const uint32_t newSharpness = std::round(d->m_handle->sharpnessSetting() * 10'000);
    if (d->m_sharpness != newSharpness) {
        d->m_sharpness = newSharpness;
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendSharpness(resource);
        }
        scheduleDone();
    }
}

void OutputDeviceV2Interface::updatePriority()
{
    if (d->m_priority != d->m_handle->priority()) {
        d->m_priority = d->m_handle->priority();
        const auto clientResources = d->resourceMap();
        for (const auto &resource : clientResources) {
            d->sendPriority(resource);
        }
        scheduleDone();
    }
}

OutputDeviceV2Interface *OutputDeviceV2Interface::get(wl_resource *native)
{
    if (auto devicePrivate = resource_cast<OutputDeviceV2InterfacePrivate *>(native); devicePrivate && !devicePrivate->isGlobalRemoved()) {
        return devicePrivate->q;
    }
    return nullptr;
}

OutputDeviceModeV2InterfacePrivate::OutputDeviceModeV2InterfacePrivate(OutputDeviceModeV2Interface *q, std::shared_ptr<OutputMode> handle)
    : QtWaylandServer::kde_output_device_mode_v2()
    , q(q)
    , m_handle(handle)
    , m_size(handle->size())
    , m_refreshRate(handle->refreshRate())
    , m_flags(handle->flags())
{
}

OutputDeviceModeV2Interface::OutputDeviceModeV2Interface(std::shared_ptr<OutputMode> handle)
    : d(new OutputDeviceModeV2InterfacePrivate(this, handle))
{
}

OutputDeviceModeV2Interface::~OutputDeviceModeV2Interface() = default;

OutputDeviceModeV2InterfacePrivate::~OutputDeviceModeV2InterfacePrivate()
{
    const auto map = resourceMap();
    for (Resource *resource : map) {
        send_removed(resource->handle);
    }
}

OutputDeviceModeV2InterfacePrivate::Resource *OutputDeviceModeV2InterfacePrivate::createResource(OutputDeviceV2InterfacePrivate::Resource *output)
{
    const auto modeResource = static_cast<ModeResource *>(add(output->client(), output->version()));
    modeResource->output = output;
    return modeResource;
}

OutputDeviceModeV2InterfacePrivate::Resource *OutputDeviceModeV2InterfacePrivate::findResource(OutputDeviceV2InterfacePrivate::Resource *output) const
{
    const auto resources = resourceMap();
    for (auto resource : resources) {
        auto modeResource = static_cast<ModeResource *>(resource);
        if (modeResource->output == output) {
            return resource;
        }
    }
    return nullptr;
}

OutputDeviceModeV2InterfacePrivate::Resource *OutputDeviceModeV2InterfacePrivate::kde_output_device_mode_v2_allocate()
{
    return new ModeResource;
}

std::weak_ptr<OutputMode> OutputDeviceModeV2Interface::handle() const
{
    return d->m_handle;
}

void OutputDeviceModeV2InterfacePrivate::bindResource(Resource *resource)
{
    send_size(resource->handle, m_size.width(), m_size.height());
    send_refresh(resource->handle, m_refreshRate);

    if (m_flags & OutputMode::Flag::Preferred) {
        send_preferred(resource->handle);
    }
    if (resource->version() >= KDE_OUTPUT_DEVICE_MODE_V2_FLAGS_CUSTOM) {
        uint32_t flags = 0;
        if (m_flags & OutputMode::Flag::Custom) {
            flags |= KDE_OUTPUT_DEVICE_MODE_V2_FLAGS_CUSTOM;
        }
        if (m_flags & OutputMode::Flag::ReducedBlanking) {
            flags |= KDE_OUTPUT_DEVICE_MODE_V2_FLAGS_REDUCED_BLANKING;
        }
        send_flags(resource->handle, flags);
    }
}

OutputDeviceModeV2Interface *OutputDeviceModeV2Interface::get(wl_resource *native)
{
    if (auto devicePrivate = resource_cast<OutputDeviceModeV2InterfacePrivate *>(native)) {
        return devicePrivate->q;
    }
    return nullptr;
}

}

#include "moc_outputdevice_v2.cpp"
