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
#include "drm_blob.h"
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

    drmModeModeInfo *nativeMode();
    std::shared_ptr<DrmBlob> blob();

    bool operator==(const DrmConnectorMode &otherMode);

private:
    DrmConnector *m_connector;
    drmModeModeInfo m_nativeMode;
    std::shared_ptr<DrmBlob> m_blob;
};

class DrmConnector : public DrmObject
{
public:
    DrmConnector(DrmGpu *gpu, uint32_t connectorId);

    bool updateProperties() override;
    void disable(DrmAtomicCommit *commit) override;

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

    enum class UnderscanOptions : uint64_t {
        Off = 0,
        On = 1,
        Auto = 2,
    };
    enum class BroadcastRgbOptions : uint64_t {
        Automatic = 0,
        Full = 1,
        Limited = 2
    };
    enum class LinkStatus : uint64_t {
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
    enum class PanelOrientation : uint64_t {
        Normal = 0,
        UpsideDown = 1,
        LeftUp = 2,
        RightUp = 3
    };
    enum class ScalingMode : uint64_t {
        None = 0,
        Full = 1,
        Center = 2,
        Full_Aspect = 3
    };
    DrmProperty crtcId;
    DrmProperty nonDesktop;
    DrmProperty dpms;
    DrmProperty edidProp;
    DrmProperty overscan;
    DrmProperty vrrCapable;
    DrmEnumProperty<UnderscanOptions> underscan;
    DrmProperty underscanVBorder;
    DrmProperty underscanHBorder;
    DrmEnumProperty<BroadcastRgbOptions> broadcastRGB;
    DrmProperty maxBpc;
    DrmEnumProperty<LinkStatus> linkStatus;
    DrmEnumProperty<DrmContentType> contentType;
    DrmEnumProperty<PanelOrientation> panelOrientation;
    DrmProperty hdrMetadata;
    DrmEnumProperty<ScalingMode> scalingMode;

    static DrmContentType kwinToDrmContentType(ContentType type);
    static Output::Transform toKWinTransform(PanelOrientation orientation);
    static BroadcastRgbOptions rgbRangeToBroadcastRgb(Output::RgbRange rgbRange);
    static Output::RgbRange broadcastRgbToRgbRange(BroadcastRgbOptions rgbRange);

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
