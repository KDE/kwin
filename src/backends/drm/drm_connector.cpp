/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_connector.h"
#include "drm_commit.h"
#include "drm_crtc.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_output.h"
#include "drm_pipeline.h"
#include "drm_pointer.h"

#include <cerrno>
#include <cstring>
#include <libxcvt/libxcvt.h>

namespace KWin
{

static QSize resolutionForMode(const drmModeModeInfo *info)
{
    return QSize(info->hdisplay, info->vdisplay);
}

static quint64 refreshRateForMode(_drmModeModeInfo *m)
{
    // Calculate higher precision (mHz) refresh rate
    // logic based on Weston, see compositor-drm.c
    quint64 refreshRate = (m->clock * 1000000LL / m->htotal + m->vtotal / 2) / m->vtotal;
    if (m->flags & DRM_MODE_FLAG_INTERLACE) {
        refreshRate *= 2;
    }
    if (m->flags & DRM_MODE_FLAG_DBLSCAN) {
        refreshRate /= 2;
    }
    if (m->vscan > 1) {
        refreshRate /= m->vscan;
    }
    return refreshRate;
}

static OutputMode::Flags flagsForMode(const drmModeModeInfo *info, OutputMode::Flags additionalFlags)
{
    OutputMode::Flags flags = additionalFlags;
    if (info->type & DRM_MODE_TYPE_PREFERRED) {
        flags |= OutputMode::Flag::Preferred;
    }
    return flags;
}

DrmConnectorMode::DrmConnectorMode(DrmConnector *connector, drmModeModeInfo nativeMode, Flags additionalFlags)
    : OutputMode(resolutionForMode(&nativeMode), refreshRateForMode(&nativeMode), flagsForMode(&nativeMode, additionalFlags))
    , m_connector(connector)
    , m_nativeMode(nativeMode)
{
}

std::shared_ptr<DrmBlob> DrmConnectorMode::blob()
{
    if (!m_blob) {
        m_blob = DrmBlob::create(m_connector->gpu(), &m_nativeMode, sizeof(m_nativeMode));
    }
    return m_blob;
}

std::chrono::nanoseconds DrmConnectorMode::vblankTime() const
{
    return std::chrono::nanoseconds(((m_nativeMode.vtotal - m_nativeMode.vdisplay) * m_nativeMode.htotal * 1'000'000ULL) / m_nativeMode.clock);
}

drmModeModeInfo *DrmConnectorMode::nativeMode()
{
    return &m_nativeMode;
}

static inline bool checkIfEqual(const drmModeModeInfo *one, const drmModeModeInfo *two)
{
    // NOTE that
    // - the struct contains a name, so doing memcmp would yield false negatives!
    // - vrefresh is a redundant value that the kernel seems to round differently from us,
    //   so that's not checked either
    return one->clock == two->clock
        && one->hdisplay == two->hdisplay
        && one->hsync_start == two->hsync_start
        && one->hsync_end == two->hsync_end
        && one->htotal == two->htotal
        && one->hskew == two->hskew
        && one->vdisplay == two->vdisplay
        && one->vsync_start == two->vsync_start
        && one->vsync_end == two->vsync_end
        && one->vtotal == two->vtotal
        && one->vscan == two->vscan;
}

bool DrmConnectorMode::operator==(const DrmConnectorMode &otherMode)
{
    return checkIfEqual(&m_nativeMode, &otherMode.m_nativeMode);
}

bool DrmConnectorMode::operator==(const drmModeModeInfo &otherMode)
{
    return checkIfEqual(&m_nativeMode, &otherMode);
}

DrmConnector::DrmConnector(DrmGpu *gpu, uint32_t connectorId)
    : DrmObject(gpu, connectorId, DRM_MODE_OBJECT_CONNECTOR)
    , crtcId(this, QByteArrayLiteral("CRTC_ID"))
    , nonDesktop(this, QByteArrayLiteral("non-desktop"))
    , dpms(this, QByteArrayLiteral("DPMS"))
    , edidProp(this, QByteArrayLiteral("EDID"))
    , overscan(this, QByteArrayLiteral("overscan"))
    , vrrCapable(this, QByteArrayLiteral("vrr_capable"))
    , underscan(this, QByteArrayLiteral("underscan"), {
                                                          QByteArrayLiteral("off"),
                                                          QByteArrayLiteral("on"),
                                                          QByteArrayLiteral("auto"),
                                                      })
    , underscanVBorder(this, QByteArrayLiteral("underscan vborder"))
    , underscanHBorder(this, QByteArrayLiteral("underscan hborder"))
    , broadcastRGB(this, QByteArrayLiteral("Broadcast RGB"), {
                                                                 QByteArrayLiteral("Automatic"),
                                                                 QByteArrayLiteral("Full"),
                                                                 QByteArrayLiteral("Limited 16:235"),
                                                             })
    , maxBpc(this, QByteArrayLiteral("max bpc"))
    , linkStatus(this, QByteArrayLiteral("link-status"), {
                                                             QByteArrayLiteral("Good"),
                                                             QByteArrayLiteral("Bad"),
                                                         })
    , contentType(this, QByteArrayLiteral("content type"), {
                                                               QByteArrayLiteral("No Data"),
                                                               QByteArrayLiteral("Graphics"),
                                                               QByteArrayLiteral("Photo"),
                                                               QByteArrayLiteral("Cinema"),
                                                               QByteArrayLiteral("Game"),
                                                           })
    , panelOrientation(this, QByteArrayLiteral("panel orientation"), {
                                                                         QByteArrayLiteral("Normal"),
                                                                         QByteArrayLiteral("Upside Down"),
                                                                         QByteArrayLiteral("Left Side Up"),
                                                                         QByteArrayLiteral("Right Side Up"),
                                                                     })
    , hdrMetadata(this, QByteArrayLiteral("HDR_OUTPUT_METADATA"))
    , scalingMode(this, QByteArrayLiteral("scaling mode"), {
                                                               QByteArrayLiteral("None"),
                                                               QByteArrayLiteral("Full"),
                                                               QByteArrayLiteral("Center"),
                                                               QByteArrayLiteral("Full aspect"),
                                                           })
    , colorspace(this, QByteArrayLiteral("Colorspace"), {
                                                            QByteArrayLiteral("Default"),
                                                            QByteArrayLiteral("BT709_YCC"),
                                                            QByteArrayLiteral("opRGB"),
                                                            QByteArrayLiteral("BT2020_RGB"),
                                                            QByteArrayLiteral("BT2020_YCC"),
                                                        })
    , path(this, QByteArrayLiteral("PATH"))
{
}

bool DrmConnector::init()
{
    if (!updateProperties()) {
        return false;
    }

    m_possibleCrtcs = drmModeConnectorGetPossibleCrtcs(gpu()->fd(), m_conn.get());

    return true;
}

bool DrmConnector::isConnected() const
{
    return !m_driverModes.empty() && m_conn && m_conn->connection == DRM_MODE_CONNECTED;
}

QString DrmConnector::connectorName() const
{
    const char *connectorName = drmModeGetConnectorTypeName(m_conn->connector_type);
    if (!connectorName) {
        connectorName = "Unknown";
    }
    return QStringLiteral("%1-%2").arg(connectorName).arg(m_conn->connector_type_id);
}

QString DrmConnector::modelName() const
{
    if (m_edid.serialNumber().isEmpty()) {
        return connectorName() + QLatin1Char('-') + m_edid.nameString();
    } else {
        return m_edid.nameString();
    }
}

bool DrmConnector::isInternal() const
{
    return m_conn->connector_type == DRM_MODE_CONNECTOR_LVDS || m_conn->connector_type == DRM_MODE_CONNECTOR_eDP
        || m_conn->connector_type == DRM_MODE_CONNECTOR_DSI;
}

QSize DrmConnector::physicalSize() const
{
    return m_physicalSize;
}

QByteArray DrmConnector::mstPath() const
{
    return m_mstPath;
}

QList<std::shared_ptr<DrmConnectorMode>> DrmConnector::modes() const
{
    return m_modes;
}

BackendOutput::SubPixel DrmConnector::subpixel() const
{
    switch (m_conn->subpixel) {
    case DRM_MODE_SUBPIXEL_UNKNOWN:
        return BackendOutput::SubPixel::Unknown;
    case DRM_MODE_SUBPIXEL_HORIZONTAL_RGB:
        return BackendOutput::SubPixel::Horizontal_RGB;
    case DRM_MODE_SUBPIXEL_HORIZONTAL_BGR:
        return BackendOutput::SubPixel::Horizontal_BGR;
    case DRM_MODE_SUBPIXEL_VERTICAL_RGB:
        return BackendOutput::SubPixel::Vertical_RGB;
    case DRM_MODE_SUBPIXEL_VERTICAL_BGR:
        return BackendOutput::SubPixel::Vertical_BGR;
    case DRM_MODE_SUBPIXEL_NONE:
        return BackendOutput::SubPixel::None;
    default:
        return BackendOutput::SubPixel::Unknown;
    }
}

bool DrmConnector::updateProperties()
{
    if (auto connector = drmModeGetConnector(gpu()->fd(), id())) {
        m_conn.reset(connector);
    } else {
        qCWarning(KWIN_DRM) << "drmModeGetConnector() failed:" << strerror(errno);
    }

    if (!m_conn) {
        return false;
    }
    DrmPropertyList props = queryProperties();
    crtcId.update(props);
    nonDesktop.update(props);
    dpms.update(props);
    edidProp.update(props);
    overscan.update(props);
    vrrCapable.update(props);
    underscan.update(props);
    underscanVBorder.update(props);
    underscanHBorder.update(props);
    broadcastRGB.update(props);
    maxBpc.update(props);
    linkStatus.update(props);
    contentType.update(props);
    panelOrientation.update(props);
    hdrMetadata.update(props);
    scalingMode.update(props);
    colorspace.update(props);
    path.update(props);

    if (gpu()->atomicModeSetting() && !crtcId.isValid()) {
        qCWarning(KWIN_DRM) << "Failed to update the basic connector properties (CRTC_ID)";
        return false;
    }

    // parse edid
    if (edidProp.immutableBlob()) {
        m_edid = Edid(edidProp.immutableBlob()->data, edidProp.immutableBlob()->length);
        if (!m_edid.isValid()) {
            qCWarning(KWIN_DRM) << "Couldn't parse EDID for connector" << this;
        }
    } else {
        m_edid = Edid{};
        if (m_conn->connection == DRM_MODE_CONNECTED) {
            qCWarning(KWIN_DRM) << "Could not find edid for connector" << this;
        }
    }

    // check the physical size
    if (m_edid.physicalSize().isEmpty()) {
        m_physicalSize = QSize(m_conn->mmWidth, m_conn->mmHeight);
    } else {
        m_physicalSize = m_edid.physicalSize();
    }

    // update modes
    bool equal = m_conn->count_modes == m_driverModes.count();
    for (int i = 0; equal && i < m_conn->count_modes; i++) {
        equal &= checkIfEqual(m_driverModes[i]->nativeMode(), &m_conn->modes[i]);
    }
    if (!equal && m_conn->count_modes > 0) {
        // reload modes
        m_driverModes.clear();
        for (int i = 0; i < m_conn->count_modes; i++) {
            m_driverModes.append(std::make_shared<DrmConnectorMode>(this, m_conn->modes[i], OutputMode::Flags()));
        }
        m_modes.clear();
        m_modes.append(m_driverModes);
        if (scalingMode.isValid() && scalingMode.hasEnum(ScalingMode::Full_Aspect)) {
            m_modes.append(generateCommonModes());
        }
    }

    m_mstPath.clear();
    if (auto blob = path.immutableBlob()) {
        QByteArray value = QByteArray(static_cast<const char *>(blob->data), blob->length);
        if (value.startsWith("mst:")) {
            // for backwards compatibility reasons the string also contains the drm connector id
            // remove that to get a more stable identifier
            const ssize_t firstHyphen = value.indexOf('-');
            if (firstHyphen > 0) {
                m_mstPath = value.mid(firstHyphen);
            } else {
                qCWarning(KWIN_DRM) << "Unexpected format in path property:" << value;
            }
        } else {
            qCWarning(KWIN_DRM) << "Unknown path type detected:" << value;
        }
    }

    return true;
}

bool DrmConnector::isCrtcSupported(DrmCrtc *crtc) const
{
    return (m_possibleCrtcs & (1 << crtc->pipeIndex()));
}

bool DrmConnector::isNonDesktop() const
{
    return nonDesktop.isValid() && nonDesktop.value() == 1;
}

const Edid *DrmConnector::edid() const
{
    return &m_edid;
}

void DrmConnector::disable(DrmAtomicCommit *commit)
{
    commit->addProperty(crtcId, 0);
}

static const QList<QSize> s_commonModes = {
    /* 4:3 (1.33) */
    QSize(1600, 1200),
    QSize(1280, 1024), /* 5:4 (1.25) */
    QSize(1024, 768),
    /* 16:10 (1.6) */
    QSize(2560, 1600),
    QSize(1920, 1200),
    QSize(1280, 800),
    /* 16:9 (1.77) */
    QSize(5120, 2880),
    QSize(3840, 2160),
    QSize(3200, 1800),
    QSize(2880, 1620),
    QSize(2560, 1440),
    QSize(1920, 1080),
    QSize(1600, 900),
    QSize(1368, 768),
    QSize(1280, 720),
};

QList<std::shared_ptr<DrmConnectorMode>> DrmConnector::generateCommonModes()
{
    QList<std::shared_ptr<DrmConnectorMode>> ret;
    QSize maxSize;
    uint64_t maxSizeRefreshRate = 0;
    for (const auto &mode : std::as_const(m_driverModes)) {
        if (mode->size().width() >= maxSize.width() && mode->size().height() >= maxSize.height() && mode->refreshRate() >= maxSizeRefreshRate) {
            maxSize = mode->size();
            maxSizeRefreshRate = mode->refreshRate();
        }
    }
    const uint64_t maxBandwidthEstimation = maxSize.width() * maxSize.height() * maxSizeRefreshRate;
    QList<uint64_t> refreshRates = {60000ul};
    if (maxSizeRefreshRate > 60000ul) {
        refreshRates.push_back(maxSizeRefreshRate);
    }
    for (const auto size : s_commonModes) {
        for (uint64_t refreshRate : refreshRates) {
            const uint64_t bandwidthEstimation = size.width() * size.height() * refreshRate;
            if (size.width() > maxSize.width() || size.height() > maxSize.height() || bandwidthEstimation > maxBandwidthEstimation) {
                continue;
            }
            const auto generatedMode = generateMode(size, refreshRate / 1000.0, OutputMode::Flags{});
            const bool alreadyExists = std::ranges::any_of(m_driverModes, [generatedMode](const auto &mode) {
                return mode->size() == generatedMode->size()
                    && std::round(mode->refreshRate() / 1000.0) == std::round(generatedMode->refreshRate() / 1000.0);
            });
            if (alreadyExists) {
                continue;
            }
            ret.push_back(generatedMode);
        }
    }
    return ret;
}

std::shared_ptr<DrmConnectorMode> DrmConnector::generateMode(const QSize &size, float refreshRate, OutputMode::Flags flags)
{
    auto modeInfo = libxcvt_gen_mode_info(size.width(), size.height(), refreshRate, flags & OutputMode::Flag::ReducedBlanking, false);

    drmModeModeInfo mode{
        .clock = uint32_t(modeInfo->dot_clock),
        .hdisplay = uint16_t(modeInfo->hdisplay),
        .hsync_start = modeInfo->hsync_start,
        .hsync_end = modeInfo->hsync_end,
        .htotal = modeInfo->htotal,
        .vdisplay = uint16_t(modeInfo->vdisplay),
        .vsync_start = modeInfo->vsync_start,
        .vsync_end = modeInfo->vsync_end,
        .vtotal = modeInfo->vtotal,
        .vscan = 1,
        .vrefresh = uint32_t(modeInfo->vrefresh),
        .flags = modeInfo->mode_flags,
        .type = DRM_MODE_TYPE_USERDEF,
    };

    sprintf(mode.name, "%dx%d@%d", size.width(), size.height(), mode.vrefresh);

    free(modeInfo);
    return std::make_shared<DrmConnectorMode>(this, mode, flags | OutputMode::Flag::Generated);
}

QDebug &operator<<(QDebug &s, const KWin::DrmConnector *obj)
{
    QDebugStateSaver saver(s);
    if (obj) {

        QString connState = QStringLiteral("Disconnected");
        if (!obj->m_conn || obj->m_conn->connection == DRM_MODE_UNKNOWNCONNECTION) {
            connState = QStringLiteral("Unknown Connection");
        } else if (obj->m_conn->connection == DRM_MODE_CONNECTED) {
            connState = QStringLiteral("Connected");
        }

        s.nospace() << "DrmConnector(id=" << obj->id() << ", gpu=" << obj->gpu() << ", name=" << obj->connectorName() << ", connection=" << connState << ", countMode=" << (obj->m_conn ? obj->m_conn->count_modes : 0)
                    << ')';
    } else {
        s << "DrmConnector(0x0)";
    }
    return s;
}

DrmConnector::DrmContentType DrmConnector::kwinToDrmContentType(ContentType type)
{
    switch (type) {
    case ContentType::None:
        return DrmContentType::Graphics;
    case ContentType::Photo:
        return DrmContentType::Photo;
    case ContentType::Video:
        return DrmContentType::Cinema;
    case ContentType::Game:
        return DrmContentType::Game;
    default:
        Q_UNREACHABLE();
    }
}

OutputTransform DrmConnector::toKWinTransform(PanelOrientation orientation)
{
    switch (orientation) {
    case PanelOrientation::Normal:
        return KWin::OutputTransform::Normal;
    case PanelOrientation::RightUp:
        return KWin::OutputTransform::Rotate270;
    case PanelOrientation::LeftUp:
        return KWin::OutputTransform::Rotate90;
    case PanelOrientation::UpsideDown:
        return KWin::OutputTransform::Rotate180;
    default:
        Q_UNREACHABLE();
    }
}

DrmConnector::BroadcastRgbOptions DrmConnector::rgbRangeToBroadcastRgb(BackendOutput::RgbRange rgbRange)
{
    switch (rgbRange) {
    case BackendOutput::RgbRange::Automatic:
        return BroadcastRgbOptions::Automatic;
    case BackendOutput::RgbRange::Full:
        return BroadcastRgbOptions::Full;
    case BackendOutput::RgbRange::Limited:
        return BroadcastRgbOptions::Limited;
    default:
        Q_UNREACHABLE();
    }
}

BackendOutput::RgbRange DrmConnector::broadcastRgbToRgbRange(BroadcastRgbOptions rgbRange)
{
    switch (rgbRange) {
    case BroadcastRgbOptions::Automatic:
        return BackendOutput::RgbRange::Automatic;
    case BroadcastRgbOptions::Full:
        return BackendOutput::RgbRange::Full;
    case BroadcastRgbOptions::Limited:
        return BackendOutput::RgbRange::Limited;
    default:
        Q_UNREACHABLE();
    }
}
}
