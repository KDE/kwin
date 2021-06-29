/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_object_connector.h"
#include "drm_gpu.h"
#include "drm_pointer.h"
#include "logging.h"

#include <main.h>
// frameworks
#include <KConfigGroup>

#include <cerrno>

namespace KWin
{

DrmConnector::DrmConnector(DrmGpu *gpu, uint32_t connectorId)
    : DrmObject(gpu, connectorId)
    , m_conn(drmModeGetConnector(gpu->fd(), connectorId))
{
    if (m_conn) {
        for (int i = 0; i < m_conn->count_encoders; ++i) {
            m_encoders << m_conn->encoders[i];
        }
    } else {
        qCWarning(KWIN_DRM) << "drmModeGetConnector failed!" << strerror(errno);
    }
}

DrmConnector::~DrmConnector() = default;

bool DrmConnector::init()
{
    if (!m_conn || !m_conn->count_modes) {
        return false;
    }
    qCDebug(KWIN_DRM) << "Creating connector" << id();

    if (!initProps({
            PropertyDefinition(QByteArrayLiteral("CRTC_ID")),
            PropertyDefinition(QByteArrayLiteral("non-desktop")),
            PropertyDefinition(QByteArrayLiteral("DPMS")),
            PropertyDefinition(QByteArrayLiteral("EDID")),
            PropertyDefinition(QByteArrayLiteral("overscan")),
            PropertyDefinition(QByteArrayLiteral("vrr_capable")),
        }, DRM_MODE_OBJECT_CONNECTOR)) {
        return false;
    }

    if (auto dpmsProp = m_props[static_cast<uint32_t>(PropertyIndex::Dpms)]) {
        dpmsProp->setLegacy();
    } else {
        qCDebug(KWIN_DRM) << "Could not find DPMS property!";
    }

    // parse edid
    auto edidProp = m_props[static_cast<uint32_t>(PropertyIndex::Edid)];
    if (edidProp && edidProp->blob() && edidProp->blob()->data) {
        m_edid = Edid(edidProp->blob()->data, edidProp->blob()->length);
        if (!m_edid.isValid()) {
            qCWarning(KWIN_DRM, "Couldn't parse EDID for connector with id %d", id());
        }
    } else {
        qCDebug(KWIN_DRM) << "Could not find edid for connector" << this;
    }
    deleteProp(PropertyIndex::Edid);

    // check the physical size
    if (m_edid.physicalSize().isEmpty()) {
        m_physicalSize = QSize(m_conn->mmWidth, m_conn->mmHeight);
    } else {
        m_physicalSize = m_edid.physicalSize();
    }

    // the size might be completely borked. E.g. Samsung SyncMaster 2494HS reports 160x90 while in truth it's 520x292
    // as this information is used to calculate DPI info, it's going to result in everything being huge
    const QByteArray unknown = QByteArrayLiteral("unknown");
    KConfigGroup group = kwinApp()->config()->group("EdidOverwrite").group(m_edid.eisaId().isEmpty() ? unknown : m_edid.eisaId())
                                                       .group(m_edid.monitorName().isEmpty() ? unknown : m_edid.monitorName())
                                                       .group(m_edid.serialNumber().isEmpty() ? unknown : m_edid.serialNumber());
    if (group.hasKey("PhysicalSize")) {
        const QSize overwriteSize = group.readEntry("PhysicalSize", m_physicalSize);
        qCWarning(KWIN_DRM) << "Overwriting monitor physical size for" << m_edid.eisaId() << "/" << m_edid.monitorName() << "/" << m_edid.serialNumber() << " from " << m_physicalSize << "to " << overwriteSize;
        m_physicalSize = overwriteSize;
    }

    return true;
}

bool DrmConnector::isConnected()
{
    DrmScopedPointer<drmModeConnector> con(drmModeGetConnector(gpu()->fd(), id()));
    if (!con) {
        return false;
    }
    return con->connection == DRM_MODE_CONNECTED;
}

static QHash<int, QByteArray> s_connectorNames = {
    {DRM_MODE_CONNECTOR_Unknown, QByteArrayLiteral("Unknown")},
    {DRM_MODE_CONNECTOR_VGA, QByteArrayLiteral("VGA")},
    {DRM_MODE_CONNECTOR_DVII, QByteArrayLiteral("DVI-I")},
    {DRM_MODE_CONNECTOR_DVID, QByteArrayLiteral("DVI-D")},
    {DRM_MODE_CONNECTOR_DVIA, QByteArrayLiteral("DVI-A")},
    {DRM_MODE_CONNECTOR_Composite, QByteArrayLiteral("Composite")},
    {DRM_MODE_CONNECTOR_SVIDEO, QByteArrayLiteral("SVIDEO")},
    {DRM_MODE_CONNECTOR_LVDS, QByteArrayLiteral("LVDS")},
    {DRM_MODE_CONNECTOR_Component, QByteArrayLiteral("Component")},
    {DRM_MODE_CONNECTOR_9PinDIN, QByteArrayLiteral("DIN")},
    {DRM_MODE_CONNECTOR_DisplayPort, QByteArrayLiteral("DP")},
    {DRM_MODE_CONNECTOR_HDMIA, QByteArrayLiteral("HDMI-A")},
    {DRM_MODE_CONNECTOR_HDMIB, QByteArrayLiteral("HDMI-B")},
    {DRM_MODE_CONNECTOR_TV, QByteArrayLiteral("TV")},
    {DRM_MODE_CONNECTOR_eDP, QByteArrayLiteral("eDP")},
    {DRM_MODE_CONNECTOR_VIRTUAL, QByteArrayLiteral("Virtual")},
    {DRM_MODE_CONNECTOR_DSI, QByteArrayLiteral("DSI")},
#ifdef DRM_MODE_CONNECTOR_DPI
    {DRM_MODE_CONNECTOR_DPI, QByteArrayLiteral("DPI")},
#endif
};

QString DrmConnector::connectorName() const
{
    return s_connectorNames.value(m_conn->connector_type, QByteArrayLiteral("Unknown")) + QStringLiteral("-") + QString::number(m_conn->connector_type_id);
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

bool DrmConnector::hasOverscan() const
{
    return m_props[static_cast<uint32_t>(PropertyIndex::Overscan)];
}

uint32_t DrmConnector::overscan() const
{
    if (const auto &prop = m_props[static_cast<uint32_t>(PropertyIndex::Overscan)]) {
        return prop->value();
    }
    return 0;
}

void DrmConnector::setOverscan(uint32_t overscan)
{
    setValue(PropertyIndex::Overscan, overscan);
}

bool DrmConnector::vrrCapable() const
{
    if (const auto &prop = m_props[static_cast<int>(PropertyIndex::VrrCapable)]) {
        return prop->value();
    }
    return false;
}

}
