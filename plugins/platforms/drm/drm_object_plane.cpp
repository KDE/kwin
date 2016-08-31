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
#include "drm_buffer.h"
#include "drm_pointer.h"
#include "logging.h"

namespace KWin
{

DrmPlane::DrmPlane(uint32_t plane_id, int fd)
    : DrmObject(plane_id, fd)
{
}

DrmPlane::~DrmPlane() = default;

bool DrmPlane::init()
{
    qCDebug(KWIN_DRM) << "Initialize plane" << m_id;
    ScopedDrmPointer<_drmModePlane, &drmModeFreePlane> p(drmModeGetPlane(m_fd, m_id));

    if (!p) {
        qCWarning(KWIN_DRM) << "Failed to get kernel plane" << m_id;
        return false;
    }

    m_possibleCrtcs = p->possible_crtcs;

    m_formats.resize(p->count_formats);
    for (int i = 0; i < p->count_formats; i++) {
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

    drmModeObjectProperties *properties = drmModeObjectGetProperties(m_fd, m_id, DRM_MODE_OBJECT_PLANE);
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
    uint64_t value = propValue(int(PropertyIndex::Type));
    int typeCount = int(TypeIndex::Count);
    for (int i = 0; i < typeCount; i++) {
            if (m_props[int(PropertyIndex::Type)]->enumMap(i) == value) {
                    return TypeIndex(i);
            }
    }
    return TypeIndex::Overlay;
}

bool DrmPlane::isCrtcSupported(uint32_t crtc)
{
    ScopedDrmPointer<_drmModeRes, &drmModeFreeResources> res(drmModeGetResources(m_fd));
    if (!res) {
        qCWarning(KWIN_DRM) << "Failed to get drm resources";
    }
    for (int c = 0; c < res->count_crtcs; c++) {
        if (res->crtcs[c] != crtc) {
            continue;
        }
        qCDebug(KWIN_DRM) <<  "Mask " << m_possibleCrtcs << ", idx " << c;
        if (m_possibleCrtcs & (1 << c)) {
            return true;
        }
    }
    qCDebug(KWIN_DRM) <<  "CRTC" << crtc << "not supported";
    return false;
}


void DrmPlane::setFormats(uint32_t const *f, int fcount)
{
    m_formats.resize(fcount);
    for (int i = 0; i < fcount; i++) {
        m_formats[i] = *f;
    }
}

DrmObject::AtomicReturn DrmPlane::atomicReqPlanePopulate(drmModeAtomicReq *req)
{
    bool ret = true;

    if (m_next) {
        setPropValue(int(PropertyIndex::FbId), m_next->bufferId());
    } else {
        setPropValue(int(PropertyIndex::FbId), 0);
        setPropValue(int(PropertyIndex::SrcX), 0);
        setPropValue(int(PropertyIndex::SrcY), 0);
        setPropValue(int(PropertyIndex::SrcW), 0);
        setPropValue(int(PropertyIndex::SrcH), 0);
        setPropValue(int(PropertyIndex::CrtcX), 0);
        setPropValue(int(PropertyIndex::CrtcY), 0);
        setPropValue(int(PropertyIndex::CrtcW), 0);
        setPropValue(int(PropertyIndex::CrtcH), 0);
    }

    m_propsPending = 0;

    for (int i = int(PropertyIndex::SrcX); i < int(PropertyIndex::CrtcId); i++) {
        ret &= atomicAddProperty(req, i, propValue(i));
    }
    ret &= atomicAddProperty(req, int(PropertyIndex::CrtcId), m_next ? propValue(int(PropertyIndex::CrtcId)) : 0);

    if (!ret) {
        qCWarning(KWIN_DRM) << "Failed to populate atomic plane" << m_id;
        return DrmObject::AtomicReturn::Error;
    }
    if (!m_propsPending) {
        return DrmObject::AtomicReturn::NoChange;
    }
    return DrmObject::AtomicReturn::Success;
}

}
