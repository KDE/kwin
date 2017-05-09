/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Roman Gilg <subdiff@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "drm_object_plane.h"
#include "drm_backend.h"
#include "drm_buffer.h"
#include "drm_pointer.h"
#include "logging.h"

namespace KWin
{

DrmPlane::DrmPlane(uint32_t plane_id, DrmBackend *backend)
    : DrmObject(plane_id, backend)
{
}

DrmPlane::~DrmPlane()
{
    delete m_current;
    delete m_next;
}

bool DrmPlane::atomicInit()
{
    qCDebug(KWIN_DRM) << "Atomic init for plane:" << m_id;
    ScopedDrmPointer<_drmModePlane, &drmModeFreePlane> p(drmModeGetPlane(m_backend->fd(), m_id));

    if (!p) {
        qCWarning(KWIN_DRM) << "Failed to get kernel plane" << m_id;
        return false;
    }

    m_possibleCrtcs = p->possible_crtcs;

    int count_formats = p->count_formats;
    m_formats.resize(count_formats);
    for (int i = 0; i < count_formats; i++) {
        m_formats[i] = p->formats[i];
    }

    if (!initProps()) {
        return false;
    }
    return true;
}

bool DrmPlane::initProps()
{
    m_propsNames = {
        QByteArrayLiteral("type"),
        QByteArrayLiteral("SRC_X"),
        QByteArrayLiteral("SRC_Y"),
        QByteArrayLiteral("SRC_W"),
        QByteArrayLiteral("SRC_H"),
        QByteArrayLiteral("CRTC_X"),
        QByteArrayLiteral("CRTC_Y"),
        QByteArrayLiteral("CRTC_W"),
        QByteArrayLiteral("CRTC_H"),
        QByteArrayLiteral("FB_ID"),
        QByteArrayLiteral("CRTC_ID"),
    };

    QVector<QByteArray> typeNames = {
        QByteArrayLiteral("Primary"),
        QByteArrayLiteral("Cursor"),
        QByteArrayLiteral("Overlay"),
    };

    drmModeObjectProperties *properties = drmModeObjectGetProperties(m_backend->fd(), m_id, DRM_MODE_OBJECT_PLANE);
    if (!properties){
        qCWarning(KWIN_DRM) << "Failed to get properties for plane " << m_id ;
        return false;
    }

    int propCount = int(PropertyIndex::Count);
    for (int j = 0; j < propCount; ++j) {
        if (j == int(PropertyIndex::Type)) {
            initProp(j, properties, typeNames);
        } else {
            initProp(j, properties);
        }
    }

    drmModeFreeObjectProperties(properties);
    return true;
}

DrmPlane::TypeIndex DrmPlane::type()
{
    uint64_t v = value(int(PropertyIndex::Type));
    int typeCount = int(TypeIndex::Count);
    for (int i = 0; i < typeCount; i++) {
            if (m_props[int(PropertyIndex::Type)]->enumMap(i) == v) {
                    return TypeIndex(i);
            }
    }
    return TypeIndex::Overlay;
}

void DrmPlane::setNext(DrmBuffer *b){
    setValue(int(PropertyIndex::FbId), b ? b->bufferId() : 0);
    m_next = b;
}

bool DrmPlane::atomicPopulate(drmModeAtomicReq *req)
{
    bool ret = true;

    for (int i = 1; i < m_props.size(); i++) {
        ret &= atomicAddProperty(req, i, m_props[i]->value());
    }

    if (!ret) {
        qCWarning(KWIN_DRM) << "Failed to populate atomic plane" << m_id;
        return false;
    }
    return true;
}

void DrmPlane::flipBuffer()
{
    m_current = m_next;
    m_next = nullptr;
}

void DrmPlane::flipBufferWithDelete()
{
    if (m_current != m_next) {
        delete m_current;
    }
    flipBuffer();
}

}
