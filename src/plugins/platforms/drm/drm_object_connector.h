/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_DRM_OBJECT_CONNECTOR_H
#define KWIN_DRM_OBJECT_CONNECTOR_H

#include <QSize>

#include "drm_object.h"
#include "edid.h"

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

    bool hasOverscan() const;
    uint32_t overscan() const;
    void setOverscan(uint32_t overscan);

private:
    DrmScopedPointer<drmModeConnector> m_conn;
    QVector<uint32_t> m_encoders;
    Edid m_edid;
    QSize m_physicalSize = QSize(-1, -1);

};

}

#endif

