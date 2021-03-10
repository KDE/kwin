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

bool DrmConnector::init()
{
    qCDebug(KWIN_DRM) << "Creating connector" << m_id;

    return initProps({
        PropertyDefinition(QByteArrayLiteral("CRTC_ID")),
        PropertyDefinition(QByteArrayLiteral("non-desktop")),
    }, DRM_MODE_OBJECT_CONNECTOR);
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
