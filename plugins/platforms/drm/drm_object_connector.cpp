/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_object_connector.h"
#include "drm_pointer.h"
#include "logging.h"

namespace KWin
{

DrmConnector::DrmConnector(uint32_t connector_id, int fd)
    : DrmObject(connector_id, fd)
{
    DrmScopedPointer<drmModeConnector> con(drmModeGetConnector(fd, connector_id));
    if (!con) {
        return;
    }
    for (int i = 0; i < con->count_encoders; ++i) {
        m_encoders << con->encoders[i];
    }
}

DrmConnector::~DrmConnector() = default;

bool DrmConnector::atomicInit()
{
    qCDebug(KWIN_DRM) << "Creating connector" << m_id;

    if (!initProps()) {
        return false;
    }
    return true;
}

bool DrmConnector::initProps()
{
    setPropertyNames( {
        QByteArrayLiteral("CRTC_ID"),
    });

    DrmScopedPointer<drmModeObjectProperties> properties(
        drmModeObjectGetProperties(fd(), m_id, DRM_MODE_OBJECT_CONNECTOR));
    if (!properties) {
        qCWarning(KWIN_DRM) << "Failed to get properties for connector " << m_id ;
        return false;
    }

    int propCount = int(PropertyIndex::Count);
    for (int j = 0; j < propCount; ++j) {
        initProp(j, properties.data());
    }

    return true;
}

bool DrmConnector::isConnected()
{
    DrmScopedPointer<drmModeConnector> con(drmModeGetConnector(fd(), m_id));
    if (!con) {
        return false;
    }
    return con->connection == DRM_MODE_CONNECTED;
}

}
