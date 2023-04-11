/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "drm_object.h"

#include <errno.h>

#include "drm_atomic_commit.h"
#include "drm_gpu.h"
#include "drm_logging.h"
#include "drm_pointer.h"

namespace KWin
{

DrmObject::DrmObject(DrmGpu *gpu, uint32_t objectId, uint32_t objectType)
    : m_gpu(gpu)
    , m_id(objectId)
    , m_objectType(objectType)
{
}

bool DrmObject::init()
{
    return updateProperties();
}

DrmPropertyList DrmObject::queryProperties() const
{
    DrmUniquePtr<drmModeObjectProperties> properties(drmModeObjectGetProperties(m_gpu->fd(), m_id, m_objectType));
    if (!properties) {
        qCWarning(KWIN_DRM) << "Failed to get properties for object" << m_id;
        return {};
    }
    DrmPropertyList ret;
    for (uint32_t i = 0; i < properties->count_props; i++) {
        DrmUniquePtr<drmModePropertyRes> prop(drmModeGetProperty(m_gpu->fd(), properties->props[i]));
        if (!prop) {
            qCWarning(KWIN_DRM, "Getting property %d of object %d failed!", properties->props[i], m_id);
            continue;
        }
        ret.addProperty(std::move(prop), properties->prop_values[i]);
    }
    return ret;
}

uint32_t DrmObject::id() const
{
    return m_id;
}

DrmGpu *DrmObject::gpu() const
{
    return m_gpu;
}

uint32_t DrmObject::type() const
{
    return m_objectType;
}

QString DrmObject::typeName() const
{
    switch (m_objectType) {
    case DRM_MODE_OBJECT_CONNECTOR:
        return QStringLiteral("connector");
    case DRM_MODE_OBJECT_CRTC:
        return QStringLiteral("crtc");
    case DRM_MODE_OBJECT_PLANE:
        return QStringLiteral("plane");
    default:
        return QStringLiteral("unknown?");
    }
}

void DrmPropertyList::addProperty(DrmUniquePtr<drmModePropertyRes> &&prop, uint64_t value)
{
    m_properties.push_back(std::make_pair(std::move(prop), value));
}

std::optional<std::pair<DrmUniquePtr<drmModePropertyRes>, uint64_t>> DrmPropertyList::takeProperty(const QByteArray &name)
{
    const auto it = std::find_if(m_properties.begin(), m_properties.end(), [&name](const auto &pair) {
        return pair.first->name == name;
    });
    if (it != m_properties.end()) {
        auto ret = std::move(*it);
        m_properties.erase(it);
        return ret;
    } else {
        return std::nullopt;
    }
}
}

QDebug operator<<(QDebug s, const KWin::DrmObject *obj)
{
    QDebugStateSaver saver(s);
    if (obj) {
        s.nospace() << "DrmObject(id=" << obj->id() << ", gpu=" << obj->gpu() << ')';
    } else {
        s << "DrmObject(0x0)";
    }
    return s;
}
