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

DrmPlane::~DrmPlane()
{
    delete m_current;
    delete m_next;
}

bool DrmPlane::atomicInit()
{
    qCDebug(KWIN_DRM) << "Atomic init for plane:" << m_id;
    DrmScopedPointer<drmModePlane> p(drmModeGetPlane(fd(), m_id));

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

bool DrmPlane::atomicPopulate(drmModeAtomicReq *req) const
{
    return doAtomicPopulate(req, 1);
}

bool DrmPlane::initProps()
{
    setPropertyNames( {
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
        QByteArrayLiteral("rotation")
    });

    QVector<QByteArray> typeNames = {
        QByteArrayLiteral("Overlay"),
        QByteArrayLiteral("Primary"),
        QByteArrayLiteral("Cursor")
    };

    const QVector<QByteArray> rotationNames{
        QByteArrayLiteral("rotate-0"),
        QByteArrayLiteral("rotate-90"),
        QByteArrayLiteral("rotate-180"),
        QByteArrayLiteral("rotate-270"),
        QByteArrayLiteral("reflect-x"),
        QByteArrayLiteral("reflect-y")
    };

    DrmScopedPointer<drmModeObjectProperties> properties(
        drmModeObjectGetProperties(fd(), m_id, DRM_MODE_OBJECT_PLANE));
    if (!properties){
        qCWarning(KWIN_DRM) << "Failed to get properties for plane " << m_id ;
        return false;
    }

    int propCount = int(PropertyIndex::Count);
    for (int j = 0; j < propCount; ++j) {
        if (j == int(PropertyIndex::Type)) {
            initProp(j, properties.data(), typeNames);
        } else if (j == int(PropertyIndex::Rotation)) {
            initProp(j, properties.data(), rotationNames);
            m_supportedTransformations = Transformations();

            auto checkSupport = [j, this] (uint64_t value, Transformation t) {
                if (propHasEnum(j, value)) {
                    m_supportedTransformations |= t;
                }
            };
            checkSupport(0, Transformation::Rotate0);
            checkSupport(1, Transformation::Rotate90);
            checkSupport(2, Transformation::Rotate180);
            checkSupport(3, Transformation::Rotate270);
            checkSupport(4, Transformation::ReflectX);
            checkSupport(5, Transformation::ReflectY);

            qCDebug(KWIN_DRM) << "Supported Transformations on plane" << m_id << "are" << m_supportedTransformations;
        } else {
            initProp(j, properties.data());
        }
    }

    return true;
}

DrmPlane::TypeIndex DrmPlane::type()
{
    auto property = m_props.at(int(PropertyIndex::Type));
    if (!property) {
        return TypeIndex::Overlay;
    }
    int typeCount = int(TypeIndex::Count);
    for (int i = 0; i < typeCount; i++) {
            if (property->enumMap(i) == property->value()) {
                    return TypeIndex(i);
            }
    }
    return TypeIndex::Overlay;
}

void DrmPlane::setNext(DrmBuffer *b)
{
    if (auto property = m_props.at(int(PropertyIndex::FbId))) {
        property->setValue(b ? b->bufferId() : 0);
    }
    m_next = b;
}

void DrmPlane::setTransformation(Transformations t)
{
    // TODO: When being pedantic, this should go through the enum mapping. Just remember
    //       that these are the shift distance, not the shifted value.
    if (auto property = m_props.at(int(PropertyIndex::Rotation))) {
        property->setValue(int(t));
    }
}

DrmPlane::Transformations DrmPlane::transformation()
{
    if (auto property = m_props.at(int(PropertyIndex::Rotation))) {
        return Transformations(int(property->value()));
    }
    return Transformations(Transformation::Rotate0);
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
