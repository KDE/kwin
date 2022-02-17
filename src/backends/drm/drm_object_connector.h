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

#include "drm_object.h"
#include "edid.h"
#include "drm_pointer.h"
#include "abstract_wayland_output.h"

namespace KWin
{

class DrmPipeline;
class DrmConnector;

/**
 * The DrmConnectorMode class represents a native mode and the associated blob.
 */
class DrmConnectorMode
{
public:
    DrmConnectorMode(DrmConnector *connector, drmModeModeInfo nativeMode);
    ~DrmConnectorMode();

    uint32_t blobId();

    drmModeModeInfo *nativeMode();
    QSize size() const;
    uint32_t refreshRate() const;

private:
    DrmConnector *m_connector;
    drmModeModeInfo m_nativeMode;
    QSize m_size;
    uint32_t m_refreshRate;
    uint32_t m_blobId = 0;
};

class DrmConnector : public DrmObject
{
public:
    DrmConnector(DrmGpu *gpu, uint32_t connectorId);
    ~DrmConnector() override;

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

    bool init() override;
    bool needsModeset() const override;
    bool updateProperties() override;
    void disable() override;

    QVector<uint32_t> encoders() const;
    bool isConnected() const;
    bool isNonDesktop() const;
    bool isInternal() const;
    DrmPipeline *pipeline() const;

    const Edid *edid() const;
    QString connectorName() const;
    QString modelName() const;
    QSize physicalSize() const;

    DrmConnectorMode *currentMode() const;
    int currentModeIndex() const;
    QVector<DrmConnectorMode *> modes() const;
    void setModeIndex(int index);
    void findCurrentMode(drmModeModeInfo currentMode);
    void updateModes();

    AbstractWaylandOutput::SubPixel subpixel() const;
    bool hasOverscan() const;
    uint32_t overscan() const;
    bool vrrCapable() const;
    bool hasRgbRange() const;
    AbstractWaylandOutput::RgbRange rgbRange() const;
    LinkStatus linkStatus() const;

private:
    QScopedPointer<DrmPipeline> m_pipeline;
    DrmScopedPointer<drmModeConnector> m_conn;
    QVector<uint32_t> m_encoders;
    Edid m_edid;
    QSize m_physicalSize = QSize(-1, -1);
    QVector<DrmConnectorMode *> m_modes;
    int m_modeIndex = 0;

    friend QDebug& operator<<(QDebug& s, const KWin::DrmConnector *obj);
};

QDebug& operator<<(QDebug& s, const KWin::DrmConnector *obj);

}
