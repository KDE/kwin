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

drmModeModeInfo *DrmConnectorMode::nativeMode()
{
    return &m_nativeMode;
}

static inline bool checkIfEqual(const drmModeModeInfo *one, const drmModeModeInfo *two)
{
    return std::memcmp(one, two, sizeof(drmModeModeInfo)) == 0;
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
    , m_conn(drmModeGetConnector(gpu->fd(), connectorId))
    , m_pipeline(m_conn ? std::make_unique<DrmPipeline>(this) : nullptr)
{
    if (m_conn) {
        for (int i = 0; i < m_conn->count_encoders; ++i) {
            DrmUniquePtr<drmModeEncoder> enc(drmModeGetEncoder(gpu->fd(), m_conn->encoders[i]));
            if (!enc) {
                qCWarning(KWIN_DRM) << "failed to get encoder" << m_conn->encoders[i];
                continue;
            }
            m_possibleCrtcs |= enc->possible_crtcs;
        }
    } else {
        qCWarning(KWIN_DRM) << "drmModeGetConnector failed!" << strerror(errno);
    }
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

std::shared_ptr<DrmConnectorMode> DrmConnector::findMode(const drmModeModeInfo &modeInfo) const
{
    const auto it = std::find_if(m_modes.constBegin(), m_modes.constEnd(), [&modeInfo](const auto &mode) {
        return checkIfEqual(mode->nativeMode(), &modeInfo);
    });
    return it == m_modes.constEnd() ? nullptr : *it;
}

Output::SubPixel DrmConnector::subpixel() const
{
    switch (m_conn->subpixel) {
    case DRM_MODE_SUBPIXEL_UNKNOWN:
        return Output::SubPixel::Unknown;
    case DRM_MODE_SUBPIXEL_HORIZONTAL_RGB:
        return Output::SubPixel::Horizontal_RGB;
    case DRM_MODE_SUBPIXEL_HORIZONTAL_BGR:
        return Output::SubPixel::Horizontal_BGR;
    case DRM_MODE_SUBPIXEL_VERTICAL_RGB:
        return Output::SubPixel::Vertical_RGB;
    case DRM_MODE_SUBPIXEL_VERTICAL_BGR:
        return Output::SubPixel::Vertical_BGR;
    case DRM_MODE_SUBPIXEL_NONE:
        return Output::SubPixel::None;
    default:
        return Output::SubPixel::Unknown;
    }
}

bool DrmConnector::updateProperties()
{
    if (auto connector = drmModeGetConnector(gpu()->fd(), id())) {
        m_conn.reset(connector);
    } else if (!m_conn) {
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
        return false;
    }

    // parse edid
    if (edidProp.immutableBlob()) {
        m_edid = Edid(edidProp.immutableBlob()->data, edidProp.immutableBlob()->length);
        if (!m_edid.isValid()) {
            qCWarning(KWIN_DRM) << "Couldn't parse EDID for connector" << this;
        }
    } else if (m_conn->connection == DRM_MODE_CONNECTED) {
        qCDebug(KWIN_DRM) << "Could not find edid for connector" << this;
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
        if (m_pipeline->mode()) {
            if (const auto mode = findMode(*m_pipeline->mode()->nativeMode())) {
                m_pipeline->setMode(mode);
            } else {
                m_pipeline->setMode(m_modes.constFirst());
            }
        } else {
            m_pipeline->setMode(m_modes.constFirst());
        }
        m_pipeline->applyPendingChanges();
        if (m_pipeline->output()) {
            m_pipeline->output()->updateModes();
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

DrmPipeline *DrmConnector::pipeline() const
{
    return m_pipeline.get();
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
    uint32_t maxSizeRefreshRate = 0;
    for (const auto &mode : std::as_const(m_driverModes)) {
        if (mode->size().width() >= maxSize.width() && mode->size().height() >= maxSize.height() && mode->refreshRate() >= maxSizeRefreshRate) {
            maxSize = mode->size();
            maxSizeRefreshRate = mode->refreshRate();
        }
    }
    const uint64_t maxBandwidthEstimation = maxSize.width() * maxSize.height() * uint64_t(maxSizeRefreshRate);
    for (const auto &size : s_commonModes) {
        const uint64_t bandwidthEstimation = size.width() * size.height() * 60000ull;
        if (size.width() > maxSize.width() || size.height() > maxSize.height() || bandwidthEstimation > maxBandwidthEstimation) {
            continue;
        }
        const auto generatedMode = generateMode(size, 60);
        if (std::any_of(m_driverModes.cbegin(), m_driverModes.cend(), [generatedMode](const auto &mode) {
                return mode->size() == generatedMode->size() && mode->refreshRate() == generatedMode->refreshRate();
            })) {
            continue;
        }
        ret << generatedMode;
    }
    return ret;
}

std::shared_ptr<DrmConnectorMode> DrmConnector::generateMode(const QSize &size, float refreshRate)
{
    auto modeInfo = libxcvt_gen_mode_info(size.width(), size.height(), refreshRate, false, false);

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
    return std::make_shared<DrmConnectorMode>(this, mode, OutputMode::Flag::Generated);
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

        s.nospace() << "DrmConnector(id=" << obj->id() << ", gpu=" << obj->gpu() << ", name=" << obj->modelName() << ", connection=" << connState << ", countMode=" << (obj->m_conn ? obj->m_conn->count_modes : 0)
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

DrmConnector::BroadcastRgbOptions DrmConnector::rgbRangeToBroadcastRgb(Output::RgbRange rgbRange)
{
    switch (rgbRange) {
    case Output::RgbRange::Automatic:
        return BroadcastRgbOptions::Automatic;
    case Output::RgbRange::Full:
        return BroadcastRgbOptions::Full;
    case Output::RgbRange::Limited:
        return BroadcastRgbOptions::Limited;
    default:
        Q_UNREACHABLE();
    }
}

Output::RgbRange DrmConnector::broadcastRgbToRgbRange(BroadcastRgbOptions rgbRange)
{
    switch (rgbRange) {
    case BroadcastRgbOptions::Automatic:
        return Output::RgbRange::Automatic;
    case BroadcastRgbOptions::Full:
        return Output::RgbRange::Full;
    case BroadcastRgbOptions::Limited:
        return Output::RgbRange::Limited;
    default:
        Q_UNREACHABLE();
    }
}
}
