/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QPoint>
#include <QSize>

#include <QSize>

#include "core/output.h"
#include "drm_object.h"
#include "drm_pointer.h"
#include "utils/edid.h"

namespace KWin
{

class DrmPipeline;
class DrmConnector;
class DrmCrtc;

/**
 * The DrmConnectorMode class represents a native mode and the associated blob.
 */
class DrmConnectorMode : public OutputMode
{
public:
    DrmConnectorMode(DrmConnector *connector, drmModeModeInfo nativeMode);
    ~DrmConnectorMode() override;

    uint32_t blobId();
    drmModeModeInfo *nativeMode();

    bool operator==(const DrmConnectorMode &otherMode);

private:
    DrmConnector *m_connector;
    drmModeModeInfo m_nativeMode;
    uint32_t m_blobId = 0;
};

class DrmConnector : public DrmObject
{
public:
    DrmConnector(DrmGpu *gpu, uint32_t connectorId);

    enum class PropertyIndex : uint32_t {
        CrtcId = 0,
        NonDesktop = 1,
        Dpms = 2,
        Edid = 3,
        Overscan = 4,
        VrrCapable = 5,
        Underscan = 6,
        Underscan_vborder = 7,
        Underscan_hborder = 8,
        Broadcast_RGB = 9,
        MaxBpc = 10,
        LinkStatus = 11,
        ContentType = 12,
        PanelOrientation = 13,
        HdrMetadata = 14,
        ScalingMode = 15,
        Count
    };

    enum class UnderscanOptions : uint32_t {
        Off = 0,
        On = 1,
        Auto = 2,
    };
    enum class LinkStatus : uint32_t {
        Good = 0,
        Bad = 1
    };
    enum class DrmContentType : uint64_t {
        None = 0,
        Graphics = 1,
        Photo = 2,
        Cinema = 3,
        Game = 4
    };
    enum class PanelOrientation : uint32_t {
        Normal = 0,
        UpsideDown = 1,
        LeftUp = 2,
        RightUp = 3
    };
    enum class ScalingMode : uint32_t {
        None = 0,
        Full = 1,
        Center = 2,
        Full_Aspect = 3
    };

    bool init() override;
    bool updateProperties() override;
    void disable() override;

    bool isCrtcSupported(DrmCrtc *crtc) const;
    bool isConnected() const;
    bool isNonDesktop() const;
    bool isInternal() const;
    DrmPipeline *pipeline() const;

    const Edid *edid() const;
    QString connectorName() const;
    QString modelName() const;
    QSize physicalSize() const;

    QList<std::shared_ptr<DrmConnectorMode>> modes() const;
    std::shared_ptr<DrmConnectorMode> findMode(const drmModeModeInfo &modeInfo) const;

    Output::SubPixel subpixel() const;
    bool hasOverscan() const;
    uint32_t overscan() const;
    bool vrrCapable() const;
    bool hasRgbRange() const;
    Output::RgbRange rgbRange() const;
    LinkStatus linkStatus() const;
    PanelOrientation panelOrientation() const;

    static DrmContentType kwinToDrmContentType(ContentType type);
    static Output::Transform toKWinTransform(PanelOrientation orientation);

private:
    QList<std::shared_ptr<DrmConnectorMode>> generateCommonModes();
    std::shared_ptr<DrmConnectorMode> generateMode(const QSize &size, float refreshRate);

    std::unique_ptr<DrmPipeline> m_pipeline;
    DrmUniquePtr<drmModeConnector> m_conn;
    Edid m_edid;
    QSize m_physicalSize = QSize(-1, -1);
    QList<std::shared_ptr<DrmConnectorMode>> m_driverModes;
    QList<std::shared_ptr<DrmConnectorMode>> m_modes;
    uint32_t m_possibleCrtcs = 0;

    friend QDebug &operator<<(QDebug &s, const KWin::DrmConnector *obj);
};

QDebug &operator<<(QDebug &s, const KWin::DrmConnector *obj);

}
