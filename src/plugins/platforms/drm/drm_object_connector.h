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

class DrmConnector : public DrmObject
{
public:
    DrmConnector(DrmGpu *gpu, uint32_t connectorId);
    ~DrmConnector() override;

    bool init() override;

    enum class PropertyIndex : uint32_t {
        CrtcId = 0,
        NonDesktop = 1,
        Dpms = 2,
        Edid = 3,
        Overscan = 4,
        Count
    };

    QVector<uint32_t> encoders() {
        return m_encoders;
    }

    bool isConnected();

    bool isNonDesktop() const {
        auto prop = m_props.at(static_cast<uint32_t>(PropertyIndex::NonDesktop));
        if (!prop) {
            return false;
        }
        return prop->value();
    }

    Property *dpms() const {
        return m_props[static_cast<uint32_t>(PropertyIndex::Dpms)];
    }

    const Edid *edid() const {
        return &m_edid;
    }

    QString connectorName() const;
    QString modelName() const;

    bool isInternal() const;
    QSize physicalSize() const;

    struct Mode {
        drmModeModeInfo mode;
        QSize size;
        uint32_t refreshRate;
    };
    /**
     * until the actual current mode is set with setMode(int)
     * this will always return the first mode
     */
    const Mode &currentMode() const;
    int currentModeIndex() const;
    const QVector<Mode> &modes();
    void setModeIndex(int index);

    AbstractWaylandOutput::SubPixel subpixel() const;

    bool hasOverscan() const;
    uint32_t overscan() const;
    void setOverscan(uint32_t overscan);

private:
    DrmScopedPointer<drmModeConnector> m_conn;
    QVector<uint32_t> m_encoders;
    Edid m_edid;
    QSize m_physicalSize = QSize(-1, -1);
    QVector<Mode> m_modes;
    int m_modeIndex = 0;

};

}

